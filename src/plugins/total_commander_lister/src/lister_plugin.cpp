#include <windows.h>

#include <algorithm>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QLatin1Char>
#include <QLatin1String>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <QtGlobal>

#include "configuration.h"
#include "lister_plugin_api.h"
#include "lister_viewer_widget.h"
#include "persistentinfo.h"

namespace klogg::tc::lister {

namespace {

struct PluginState {
    std::unique_ptr<QApplication> app;
    std::unordered_map<HWND, std::unique_ptr<ListerViewerWidget>> viewers;
    std::mutex mutex;
    bool settingsInitialized = false;
};

QFile& logFile()
{
    static QFile file;
    return file;
}

std::mutex& logMutex()
{
    static std::mutex mutex;
    return mutex;
}

QtMessageHandler& previousMessageHandler()
{
    static QtMessageHandler handler = nullptr;
    return handler;
}

QString messageTypeName( QtMsgType type )
{
    switch ( type ) {
    case QtDebugMsg:
        return QStringLiteral( "DEBUG" );
    case QtInfoMsg:
        return QStringLiteral( "INFO" );
    case QtWarningMsg:
        return QStringLiteral( "WARNING" );
    case QtCriticalMsg:
        return QStringLiteral( "CRITICAL" );
    case QtFatalMsg:
        return QStringLiteral( "FATAL" );
    }

    return QStringLiteral( "UNKNOWN" );
}

void writeLogLine( const QString& category, const QString& message )
{
    auto& file = logFile();
    if ( !file.isOpen() ) {
        return;
    }

    std::lock_guard<std::mutex> lock( logMutex() );
    QTextStream stream( &file );
    stream << QDateTime::currentDateTime().toString( Qt::ISODateWithMs ) << " [" << category << "] "
           << message << Qt::endl;
    stream.flush();
}

void qtLogHandler( QtMsgType type, const QMessageLogContext& context, const QString& msg )
{
    QStringList fragments;
    fragments << messageTypeName( type );

    if ( context.file && context.file[0] ) {
        fragments << QString::fromUtf8( context.file ) + QLatin1Char( ':' ) + QString::number( context.line );
    }

    writeLogLine( fragments.join( QLatin1String( "|" ) ), msg );

    if ( auto handler = previousMessageHandler() ) {
        handler( type, context, msg );
    }
}

QString resolveLogPath()
{
    const QString environmentPath = qEnvironmentVariable( "KLOGG_LISTER_LOG" ).trimmed();
    if ( !environmentPath.isEmpty() ) {
        return environmentPath;
    }

    wchar_t tempPathBuffer[MAX_PATH];
    const DWORD length = GetTempPathW( MAX_PATH, tempPathBuffer );
    if ( length == 0 || length >= MAX_PATH ) {
        return {};
    }

    QString basePath = QString::fromWCharArray( tempPathBuffer, static_cast<int>( length ) );
    QDir dir( basePath );
    return dir.filePath( QStringLiteral( "klogg_lister_qt.log" ) );
}

void initializeLogging()
{
    static bool initialized = false;
    if ( initialized ) {
        return;
    }
    initialized = true;

    qputenv( "QT_DEBUG_PLUGINS", QByteArrayLiteral( "1" ) );

    const QString logPath = resolveLogPath();
    if ( logPath.isEmpty() ) {
        return;
    }

    QFileInfo info( logPath );
    QDir directory = info.dir();
    if ( !directory.exists() ) {
        directory.mkpath( QStringLiteral( "." ) );
    }

    auto& file = logFile();
    file.setFileName( logPath );
    if ( !file.open( QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text ) ) {
        return;
    }

    writeLogLine( QStringLiteral( "plugin" ),
                  QStringLiteral( "logging initialized at %1" ).arg( QDir::toNativeSeparators( logPath ) ) );

    previousMessageHandler() = qInstallMessageHandler( qtLogHandler );
}

QString pluginDirectory()
{
    static QString directory;
    static bool resolved = false;
    if ( resolved ) {
        return directory;
    }
    resolved = true;

    HMODULE module = nullptr;
    if ( GetModuleHandleExW( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             reinterpret_cast<LPCWSTR>( &pluginDirectory ), &module ) ) {
        wchar_t modulePath[MAX_PATH];
        const DWORD length = GetModuleFileNameW( module, modulePath, MAX_PATH );
        if ( length != 0 && length < MAX_PATH ) {
            QFileInfo info( QString::fromWCharArray( modulePath, static_cast<int>( length ) ) );
            directory = info.absolutePath();
            writeLogLine( QStringLiteral( "DEBUG" ),
                          QStringLiteral( "plugin directory resolved to '%1'" )
                              .arg( QDir::toNativeSeparators( directory ) ) );
        } else {
            writeLogLine( QStringLiteral( "WARNING" ),
                          QStringLiteral( "failed to resolve plugin directory, error=%1" )
                              .arg( QString::number( GetLastError() ) ) );
        }
    } else {
        writeLogLine( QStringLiteral( "WARNING" ),
                      QStringLiteral( "GetModuleHandleExW failed, error=%1" )
                          .arg( QString::number( GetLastError() ) ) );
    }

    return directory;
}

QString ensureQtPlatformPluginPath()
{
    static bool attempted = false;
    static QString platformPath;
    if ( attempted ) {
        return platformPath;
    }
    attempted = true;

    const QString baseDirectory = pluginDirectory();
    QStringList candidateDirectories;
    if ( !baseDirectory.isEmpty() ) {
        candidateDirectories << QDir( baseDirectory ).filePath( QStringLiteral( "platforms" ) );
        candidateDirectories << QDir( baseDirectory ).filePath( QStringLiteral( "qt6/plugins/platforms" ) );
        candidateDirectories << QDir( baseDirectory ).filePath( QStringLiteral( "qt5/plugins/platforms" ) );
    }

    for ( const QString& candidate : candidateDirectories ) {
        if ( candidate.isEmpty() ) {
            continue;
        }

        const QString normalizedPath = QDir( candidate ).absolutePath();
        writeLogLine( QStringLiteral( "DEBUG" ),
                      QStringLiteral( "checking directory path '%1' ..." )
                          .arg( QDir::toNativeSeparators( normalizedPath ) ) );

        const QString pluginFile = QDir( normalizedPath ).filePath( QStringLiteral( "qwindows.dll" ) );
        if ( QFileInfo::exists( pluginFile ) ) {
            platformPath = normalizedPath;
            const QByteArray encodedPath = QDir::toNativeSeparators( platformPath ).toLocal8Bit();
            qputenv( "QT_QPA_PLATFORM_PLUGIN_PATH", encodedPath );
            writeLogLine( QStringLiteral( "plugin" ),
                          QStringLiteral( "Qt platform plugin path set to '%1'" )
                              .arg( QDir::toNativeSeparators( platformPath ) ) );
            break;
        }

        writeLogLine( QStringLiteral( "WARNING" ),
                      QStringLiteral( "Could not find the Qt platform plugin 'windows' in '%1'" )
                          .arg( QDir::toNativeSeparators( normalizedPath ) ) );
    }

    if ( platformPath.isEmpty() ) {
        writeLogLine( QStringLiteral( "WARNING" ),
                      QStringLiteral( "Failed to locate a Qt platform plugins directory" ) );
    }

    return platformPath;
}

void ensureQtPlatformLibraryPath( const QString& platformPath )
{
    static bool added = false;
    if ( added ) {
        return;
    }

    auto addLibraryPath = []( const QString& path ) {
        if ( path.isEmpty() ) {
            return false;
        }

        const QString normalizedPath = QDir( path ).absolutePath();
        const auto paths = QCoreApplication::libraryPaths();
        if ( std::any_of( paths.cbegin(), paths.cend(),
                          [&]( const QString& existing ) {
                              return existing.compare( normalizedPath, Qt::CaseInsensitive ) == 0;
                          } ) ) {
            return false;
        }

        QCoreApplication::addLibraryPath( normalizedPath );
        writeLogLine( QStringLiteral( "plugin" ),
                      QStringLiteral( "Qt library search path augmented with '%1'" )
                          .arg( QDir::toNativeSeparators( normalizedPath ) ) );
        return true;
    };

    addLibraryPath( platformPath );
    addLibraryPath( pluginDirectory() );

    added = true;
}

PluginState& state()
{
    static PluginState instance;
    return instance;
}

void processQtEvents()
{
    if ( auto* dispatcher = QCoreApplication::instance() ) {
        QCoreApplication::processEvents( QEventLoop::AllEvents, 50 );
    }
}

void ensureQtApplication()
{
    initializeLogging();
    writeLogLine( QStringLiteral( "plugin" ), QStringLiteral( "ensuring Qt application" ) );

    const QString platformPath = ensureQtPlatformPluginPath();
    auto& st = state();
    if ( !QCoreApplication::instance() ) {
        int argc = 0;
        static char appName[] = "klogg_lister";
        static char* argv[] = { appName, nullptr };
        st.app = std::make_unique<QApplication>( argc, argv );
        writeLogLine( QStringLiteral( "plugin" ), QStringLiteral( "QApplication created" ) );
    }

    ensureQtPlatformLibraryPath( platformPath );

    if ( !st.settingsInitialized ) {
        st.settingsInitialized = true;
        PersistentInfo::overrideApplicationKeys( QStringLiteral( "klogg_lister" ),
                                                 QStringLiteral( "klogg_lister_session" ) );
        PersistentInfo::overridePortableMode( true );
        Configuration::getSynced();
        writeLogLine( QStringLiteral( "plugin" ), QStringLiteral( "settings initialized" ) );
    }
}

QString toQString( const char* path )
{
    return QString::fromLocal8Bit( path ? path : "" );
}

QString toQString( const wchar_t* path )
{
    return QString::fromWCharArray( path ? path : L"" );
}

HWND createViewerWindow( HWND parent, const QString& filePath, int showFlags )
{
    ensureQtApplication();

    writeLogLine( QStringLiteral( "plugin" ),
                  QStringLiteral( "creating viewer window for '%1'" ).arg( QDir::toNativeSeparators( filePath ) ) );

    auto viewer = std::make_unique<ListerViewerWidget>();
    viewer->setAttribute( Qt::WA_NativeWindow );
    viewer->setWindowFlag( Qt::FramelessWindowHint );
    viewer->applyShowFlags( showFlags );

    if ( !viewer->loadFile( filePath ) ) {
        writeLogLine( QStringLiteral( "plugin" ),
                      QStringLiteral( "failed to load file '%1'" ).arg( QDir::toNativeSeparators( filePath ) ) );
        return nullptr;
    }

    HWND hwnd = reinterpret_cast<HWND>( viewer->winId() );
    viewer->show();
    SetParent( hwnd, parent );
    SetWindowLongPtr( hwnd, GWL_STYLE, WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE );
    ShowWindow( hwnd, SW_SHOW );

    auto& st = state();
    {
        std::lock_guard<std::mutex> lock( st.mutex );
        st.viewers.emplace( hwnd, std::move( viewer ) );
    }

    processQtEvents();
    return hwnd;
}

ListerViewerWidget* findViewer( HWND hwnd )
{
    auto& st = state();
    std::lock_guard<std::mutex> lock( st.mutex );
    auto it = st.viewers.find( hwnd );
    if ( it == st.viewers.end() ) {
        return nullptr;
    }

    return it->second.get();
}

int loadNextFile( HWND hwnd, const QString& filePath, int showFlags )
{
    ensureQtApplication();

    if ( auto* viewer = findViewer( hwnd ) ) {
        viewer->applyShowFlags( showFlags );
        const bool ok = viewer->loadNextFile( filePath );
        processQtEvents();
        return ok ? kResultOk : kResultError;
    }

    return kResultError;
}

int closeViewer( HWND hwnd )
{
    auto& st = state();
    std::unique_ptr<ListerViewerWidget> widget;
    {
        std::lock_guard<std::mutex> lock( st.mutex );
        auto it = st.viewers.find( hwnd );
        if ( it == st.viewers.end() ) {
            return kResultError;
        }
        widget = std::move( it->second );
        st.viewers.erase( it );
    }

    if ( widget ) {
        widget->closeFile();
        delete widget.release();
    }

    processQtEvents();
    return kResultOk;
}

int searchText( HWND hwnd, const QString& text, int parameters )
{
    ensureQtApplication();

    if ( auto* viewer = findViewer( hwnd ) ) {
        const bool ok = viewer->searchText( text, parameters );
        processQtEvents();
        return ok ? kResultOk : kResultError;
    }

    return kResultError;
}

int sendCommand( HWND hwnd, int command, int parameter )
{
    ensureQtApplication();

    if ( auto* viewer = findViewer( hwnd ) ) {
        const int result = viewer->sendCommand( command, parameter );
        processQtEvents();
        return result;
    }

    return kResultError;
}

} // namespace

} // namespace klogg::tc::lister

extern "C" {

__declspec( dllexport ) HWND __stdcall ListLoad( HWND parentWin, char* fileToLoad, int showFlags )
{
    return klogg::tc::lister::createViewerWindow( parentWin,
                                                  klogg::tc::lister::toQString( fileToLoad ), showFlags );
}

__declspec( dllexport ) HWND __stdcall ListLoadW( HWND parentWin, const wchar_t* fileToLoad, int showFlags )
{
    return klogg::tc::lister::createViewerWindow( parentWin,
                                                  klogg::tc::lister::toQString( fileToLoad ), showFlags );
}

__declspec( dllexport ) int __stdcall ListLoadNext( HWND parentWin, HWND pluginWin, char* fileToLoad,
                                                  int showFlags )
{
    Q_UNUSED( parentWin );
    return klogg::tc::lister::loadNextFile( pluginWin, klogg::tc::lister::toQString( fileToLoad ),
                                            showFlags );
}

__declspec( dllexport ) int __stdcall ListLoadNextW( HWND parentWin, HWND pluginWin, const wchar_t* fileToLoad,
                                                     int showFlags )
{
    Q_UNUSED( parentWin );
    return klogg::tc::lister::loadNextFile( pluginWin, klogg::tc::lister::toQString( fileToLoad ),
                                            showFlags );
}

__declspec( dllexport ) void __stdcall ListCloseWindow( HWND listWin )
{
    klogg::tc::lister::closeViewer( listWin );
}

__declspec( dllexport ) int __stdcall ListSearchText( HWND listWin, char* searchString, int searchParameter )
{
    return klogg::tc::lister::searchText( listWin, klogg::tc::lister::toQString( searchString ),
                                          searchParameter );
}

__declspec( dllexport ) int __stdcall ListSearchTextW( HWND listWin, const wchar_t* searchString,
                                                       int searchParameter )
{
    return klogg::tc::lister::searchText( listWin, klogg::tc::lister::toQString( searchString ),
                                          searchParameter );
}

__declspec( dllexport ) int __stdcall ListSendCommand( HWND listWin, int command, int parameter )
{
    return klogg::tc::lister::sendCommand( listWin, command, parameter );
}

__declspec( dllexport ) int __stdcall ListNotificationReceived( HWND listWin, int message, WPARAM wParam,
                                                                LPARAM lParam )
{
    Q_UNUSED( listWin );
    Q_UNUSED( message );
    Q_UNUSED( wParam );
    Q_UNUSED( lParam );
    return klogg::tc::lister::kResultNotImplemented;
}

} // extern "C"
