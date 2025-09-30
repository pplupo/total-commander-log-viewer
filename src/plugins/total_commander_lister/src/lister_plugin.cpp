#include <windows.h>

#include <algorithm>
#include <exception>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <cstring>

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QLibraryInfo>
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
    std::mutex viewersMutex;
    std::mutex initMutex;
    bool settingsInitialized = false;
    bool initializationFailed = false;
    QString initializationError;
};

QFile& logFile()
{
    static QFile file;
    return file;
}

void writeLogLine( const QString& category, const QString& message );

const QStringList& supportedExtensions()
{
    static const QStringList extensions = { QStringLiteral( "LOG" ),  QStringLiteral( "LOGX" ),
                                            QStringLiteral( "LOGS" ), QStringLiteral( "CEF" ),
                                            QStringLiteral( "CLF" ),  QStringLiteral( "ELF" ),
                                            QStringLiteral( "W3C" ),  QStringLiteral( "OUT" ),
                                            QStringLiteral( "ERR" ) };
    return extensions;
}

const QString& detectString()
{
    static const QString detect = []() {
        QStringList parts;
        parts.reserve( supportedExtensions().size() );
        for ( const auto& extension : supportedExtensions() ) {
            parts << QStringLiteral( "EXT=\"%1\"" ).arg( extension );
        }
        return parts.join( QLatin1String( " | " ) );
    }();

    return detect;
}

int populateDetectString( char* buffer, int maxLength )
{
    if ( buffer == nullptr ) {
        writeLogLine( QStringLiteral( "plugin" ),
                      QStringLiteral( "ListGetDetectString aborted: null buffer" ) );
        return kResultError;
    }

    if ( maxLength <= 0 ) {
        writeLogLine( QStringLiteral( "plugin" ),
                      QStringLiteral( "ListGetDetectString aborted: invalid max length %1" )
                          .arg( QString::number( maxLength ) ) );
        return kResultError;
    }

    const QByteArray asciiDetect = detectString().toUtf8();
    const int available = maxLength - 1;
    const int copyLength = static_cast<int>( std::min<qsizetype>(
        static_cast<qsizetype>( available ), asciiDetect.size() ) );

    if ( copyLength > 0 ) {
        std::memcpy( buffer, asciiDetect.constData(), static_cast<size_t>( copyLength ) );
    }

    buffer[ copyLength ] = '\0';

    if ( copyLength < asciiDetect.size() ) {
        writeLogLine( QStringLiteral( "plugin" ),
                      QStringLiteral( "ListGetDetectString truncated to '%1'" )
                          .arg( QString::fromUtf8( asciiDetect.left( copyLength ) ) ) );
        return kResultError;
    }

    writeLogLine( QStringLiteral( "plugin" ),
                  QStringLiteral( "ListGetDetectString returned '%1'" )
                      .arg( QString::fromUtf8( asciiDetect ) ) );
    return kResultOk;
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
#ifdef Q_OS_WIN
        OutputDebugStringW( L"[klogg_lister] Failed to resolve log path. Logging disabled.\n" );
#endif
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
#ifdef Q_OS_WIN
        const auto message = QStringLiteral( "[klogg_lister] Failed to open log file '%1'." )
                                 .arg( QDir::toNativeSeparators( logPath ) );
        OutputDebugStringW( reinterpret_cast<LPCWSTR>( message.utf16() ) );
#endif
        return;
    }

    writeLogLine( QStringLiteral( "plugin" ),
                  QStringLiteral( "logging initialized at %1" ).arg( QDir::toNativeSeparators( logPath ) ) );

    const QString environmentPath = qEnvironmentVariable( "KLOGG_LISTER_LOG" ).trimmed();
    if ( !environmentPath.isEmpty() ) {
        writeLogLine( QStringLiteral( "plugin" ),
                      QStringLiteral( "log path overridden by KLOGG_LISTER_LOG='%1'" )
                          .arg( QDir::toNativeSeparators( environmentPath ) ) );
    }

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

bool ensureQtApplication( QString* failureReason = nullptr )
{
    initializeLogging();

    auto& st = state();
    std::lock_guard<std::mutex> initLock( st.initMutex );

    auto setFailure = [&]( const QString& message ) {
        st.initializationFailed = true;
        st.initializationError = message;
        writeLogLine( QStringLiteral( "plugin" ), message );
        if ( failureReason ) {
            *failureReason = message;
        }
    };

    if ( st.initializationFailed ) {
        if ( failureReason ) {
            *failureReason = st.initializationError;
        }
        writeLogLine( QStringLiteral( "plugin" ),
                      QStringLiteral( "Qt initialization skipped after previous failure: %1" )
                          .arg( st.initializationError ) );
        return false;
    }

    writeLogLine( QStringLiteral( "plugin" ), QStringLiteral( "ensuring Qt application" ) );

    const QString platformPath = ensureQtPlatformPluginPath();

    if ( !QCoreApplication::instance() ) {
        if ( platformPath.isEmpty() ) {
            setFailure( QStringLiteral( "Qt platform plugin directory not found. Aborting Qt initialization." ) );
            return false;
        }

        int argc = 0;
        static char appName[] = "klogg_lister";
        static char* argv[] = { appName, nullptr };

        try {
            st.app = std::make_unique<QApplication>( argc, argv );
        }
        catch ( const std::exception& ex ) {
            setFailure( QStringLiteral( "QApplication construction failed: %1" )
                            .arg( QString::fromUtf8( ex.what() ) ) );
            return false;
        }
        catch ( ... ) {
            setFailure( QStringLiteral( "QApplication construction failed with an unknown exception" ) );
            return false;
        }

        if ( !st.app ) {
            setFailure( QStringLiteral( "Failed to allocate QApplication instance" ) );
            return false;
        }

        QApplication::setQuitOnLastWindowClosed( false );

        writeLogLine( QStringLiteral( "plugin" ), QStringLiteral( "QApplication created" ) );
        writeLogLine( QStringLiteral( "plugin" ),
                      QStringLiteral( "Qt runtime version %1 (library %2)" )
                          .arg( QString::fromLatin1( qVersion() ),
                                QLibraryInfo::version().toString() ) );
    }
    else {
        writeLogLine( QStringLiteral( "plugin" ), QStringLiteral( "reusing existing QApplication" ) );
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

    if ( failureReason ) {
        failureReason->clear();
    }

    return true;
}

QString toQString( const char* path )
{
    return QString::fromLocal8Bit( path ? path : "" );
}

QString toQString( const wchar_t* path )
{
    return QString::fromWCharArray( path ? path : L"" );
}

QString formatWindowHandle( HWND hwnd )
{
    return QStringLiteral( "0x%1" ).arg( quintptr( hwnd ), 0, 16, QLatin1Char( '0' ) );
}

QString formatShowFlags( int showFlags )
{
    return QStringLiteral( "0x%1" ).arg( showFlags, 0, 16, QLatin1Char( '0' ) );
}

HWND createViewerWindow( HWND parent, const QString& filePath, int showFlags )
{
    QString failureReason;
    if ( !ensureQtApplication( &failureReason ) ) {
        writeLogLine( QStringLiteral( "plugin" ),
                      QStringLiteral( "createViewerWindow aborted: %1" )
                          .arg( failureReason.isEmpty() ? QStringLiteral( "Qt initialization failed" )
                                                       : failureReason ) );
        return nullptr;
    }

    writeLogLine( QStringLiteral( "plugin" ),
                  QStringLiteral( "creating viewer window for '%1' (parent=%2, flags=%3)" )
                      .arg( QDir::toNativeSeparators( filePath ), formatWindowHandle( parent ),
                            formatShowFlags( showFlags ) ) );

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

    writeLogLine( QStringLiteral( "plugin" ),
                  QStringLiteral( "viewer window  %1 created for '%2'" )
                      .arg( formatWindowHandle( hwnd ), QDir::toNativeSeparators( filePath ) ) );

    auto& st = state();
    {
        std::lock_guard<std::mutex> lock( st.viewersMutex );
        st.viewers.emplace( hwnd, std::move( viewer ) );
    }

    processQtEvents();
    writeLogLine( QStringLiteral( "plugin" ),
                  QStringLiteral( "viewer window %1 initialized" ).arg( formatWindowHandle( hwnd ) ) );
    return hwnd;
}

ListerViewerWidget* findViewer( HWND hwnd )
{
    auto& st = state();
    std::lock_guard<std::mutex> lock( st.viewersMutex );
    auto it = st.viewers.find( hwnd );
    if ( it == st.viewers.end() ) {
        writeLogLine( QStringLiteral( "plugin" ),
                      QStringLiteral( "requested window %1 not found" ).arg( formatWindowHandle( hwnd ) ) );
        return nullptr;
    }

    return it->second.get();
}

int loadNextFile( HWND hwnd, const QString& filePath, int showFlags )
{
    QString failureReason;
    if ( !ensureQtApplication( &failureReason ) ) {
        writeLogLine( QStringLiteral( "plugin" ),
                      QStringLiteral( "loadNextFile aborted: %1" )
                          .arg( failureReason.isEmpty() ? QStringLiteral( "Qt initialization failed" )
                                                       : failureReason ) );
        return kResultError;
    }

    writeLogLine( QStringLiteral( "plugin" ),
                  QStringLiteral( "loading next file '%1' into window %2 (flags=%3)" )
                      .arg( QDir::toNativeSeparators( filePath ), formatWindowHandle( hwnd ),
                            formatShowFlags( showFlags ) ) );

    if ( auto* viewer = findViewer( hwnd ) ) {
        viewer->applyShowFlags( showFlags );
        const bool ok = viewer->loadNextFile( filePath );
        processQtEvents();
        writeLogLine( QStringLiteral( "plugin" ),
                      QStringLiteral( "load next file %1 for window %2" )
                          .arg( ok ? QStringLiteral( "succeeded" ) : QStringLiteral( "failed" ),
                                formatWindowHandle( hwnd ) ) );
        return ok ? kResultOk : kResultError;
    }

    writeLogLine( QStringLiteral( "plugin" ),
                  QStringLiteral( "cannot load next file into unknown window %1" )
                      .arg( formatWindowHandle( hwnd ) ) );
    return kResultError;
}

int closeViewer( HWND hwnd )
{
    auto& st = state();
    std::unique_ptr<ListerViewerWidget> widget;
    {
        std::lock_guard<std::mutex> lock( st.viewersMutex );
        auto it = st.viewers.find( hwnd );
        if ( it == st.viewers.end() ) {
            writeLogLine( QStringLiteral( "plugin" ),
                          QStringLiteral( "closeViewer requested for unknown window %1" )
                              .arg( formatWindowHandle( hwnd ) ) );
            return kResultError;
        }
        widget = std::move( it->second );
        st.viewers.erase( it );
    }

    if ( widget ) {
        widget->closeFile();
        delete widget.release();
        writeLogLine( QStringLiteral( "plugin" ),
                      QStringLiteral( "viewer window %1 closed" ).arg( formatWindowHandle( hwnd ) ) );
    }

    processQtEvents();
    return kResultOk;
}

int searchText( HWND hwnd, const QString& text, int parameters )
{
    QString failureReason;
    if ( !ensureQtApplication( &failureReason ) ) {
        writeLogLine( QStringLiteral( "plugin" ),
                      QStringLiteral( "searchText aborted: %1" )
                          .arg( failureReason.isEmpty() ? QStringLiteral( "Qt initialization failed" )
                                                       : failureReason ) );
        return kResultError;
    }

    writeLogLine( QStringLiteral( "plugin" ),
                  QStringLiteral( "search request on window %1 text='%2' params=0x%3" )
                      .arg( formatWindowHandle( hwnd ), text,
                            QString::number( parameters, 16 ) ) );

    if ( auto* viewer = findViewer( hwnd ) ) {
        const bool ok = viewer->searchText( text, parameters );
        processQtEvents();
        writeLogLine( QStringLiteral( "plugin" ),
                      QStringLiteral( "search request on window %1 %2" )
                          .arg( formatWindowHandle( hwnd ),
                                ok ? QStringLiteral( "succeeded" ) : QStringLiteral( "failed" ) ) );
        return ok ? kResultOk : kResultError;
    }

    writeLogLine( QStringLiteral( "plugin" ),
                  QStringLiteral( "cannot process search for unknown window %1" )
                      .arg( formatWindowHandle( hwnd ) ) );
    return kResultError;
}

int sendCommand( HWND hwnd, int command, int parameter )
{
    QString failureReason;
    if ( !ensureQtApplication( &failureReason ) ) {
        writeLogLine( QStringLiteral( "plugin" ),
                      QStringLiteral( "sendCommand aborted: %1" )
                          .arg( failureReason.isEmpty() ? QStringLiteral( "Qt initialization failed" )
                                                       : failureReason ) );
        return kResultError;
    }

    writeLogLine( QStringLiteral( "plugin" ),
                  QStringLiteral( "sendCommand window=%1 command=%2 parameter=%3" )
                      .arg( formatWindowHandle( hwnd ), QString::number( command ),
                            QString::number( parameter ) ) );

    if ( auto* viewer = findViewer( hwnd ) ) {
        const int result = viewer->sendCommand( command, parameter );
        processQtEvents();
        writeLogLine( QStringLiteral( "plugin" ),
                      QStringLiteral( "command %1 on window %2 returned %3" )
                          .arg( QString::number( command ), formatWindowHandle( hwnd ),
                                QString::number( result ) ) );
        return result;
    }

    writeLogLine( QStringLiteral( "plugin" ),
                  QStringLiteral( "sendCommand target window %1 not found" )
                      .arg( formatWindowHandle( hwnd ) ) );
    return kResultError;
}

template <typename Func, typename Failure>
auto invokeSafely( const char* context, Func&& func, Failure&& failure )
{
    try {
        return func();
    }
    catch ( const std::exception& ex ) {
        writeLogLine( QStringLiteral( "plugin" ),
                      QStringLiteral( "%1 threw exception: %2" )
                          .arg( QString::fromLatin1( context ), QString::fromUtf8( ex.what() ) ) );
    }
    catch ( ... ) {
        writeLogLine( QStringLiteral( "plugin" ),
                      QStringLiteral( "%1 threw unknown exception" ).arg( QString::fromLatin1( context ) ) );
    }

    return failure;
}

} // namespace

} // namespace klogg::tc::lister

extern "C" {

__declspec( dllexport ) HWND __stdcall ListLoad( HWND parentWin, char* fileToLoad, int showFlags )
{
    using namespace klogg::tc::lister;
    initializeLogging();

    const QString path = toQString( fileToLoad );
    writeLogLine( QStringLiteral( "plugin" ),
                  QStringLiteral( "ListLoad called parent=%1 file='%2' flags=%3" )
                      .arg( formatWindowHandle( parentWin ), QDir::toNativeSeparators( path ),
                            formatShowFlags( showFlags ) ) );

    return invokeSafely( "ListLoad", [ & ]() { return createViewerWindow( parentWin, path, showFlags ); },
                         static_cast<HWND>( nullptr ) );
}

__declspec( dllexport ) HWND __stdcall ListLoadW( HWND parentWin, const wchar_t* fileToLoad, int showFlags )
{
    using namespace klogg::tc::lister;
    initializeLogging();

    const QString path = toQString( fileToLoad );
    writeLogLine( QStringLiteral( "plugin" ),
                  QStringLiteral( "ListLoadW called parent=%1 file='%2' flags=%3" )
                      .arg( formatWindowHandle( parentWin ), QDir::toNativeSeparators( path ),
                            formatShowFlags( showFlags ) ) );

    return invokeSafely( "ListLoadW", [ & ]() { return createViewerWindow( parentWin, path, showFlags ); },
                         static_cast<HWND>( nullptr ) );
}

__declspec( dllexport ) int __stdcall ListLoadNext( HWND parentWin, HWND pluginWin, char* fileToLoad,
                                                   int showFlags )
{
    Q_UNUSED( parentWin );
    using namespace klogg::tc::lister;
    initializeLogging();

    const QString path = toQString( fileToLoad );
    writeLogLine( QStringLiteral( "plugin" ),
                  QStringLiteral( "ListLoadNext called window=%1 file='%2' flags=%3" )
                      .arg( formatWindowHandle( pluginWin ), QDir::toNativeSeparators( path ),
                            formatShowFlags( showFlags ) ) );

    return invokeSafely( "ListLoadNext",
                         [ & ]() { return loadNextFile( pluginWin, path, showFlags ); }, kResultError );
}

__declspec( dllexport ) int __stdcall ListLoadNextW( HWND parentWin, HWND pluginWin, const wchar_t* fileToLoad,
                                                    int showFlags )
{
    Q_UNUSED( parentWin );
    using namespace klogg::tc::lister;
    initializeLogging();

    const QString path = toQString( fileToLoad );
    writeLogLine( QStringLiteral( "plugin" ),
                  QStringLiteral( "ListLoadNextW called window=%1 file='%2' flags=%3" )
                      .arg( formatWindowHandle( pluginWin ), QDir::toNativeSeparators( path ),
                            formatShowFlags( showFlags ) ) );

    return invokeSafely( "ListLoadNextW",
                         [ & ]() { return loadNextFile( pluginWin, path, showFlags ); }, kResultError );
}

__declspec( dllexport ) void __stdcall ListCloseWindow( HWND listWin )
{
    using namespace klogg::tc::lister;
    initializeLogging();

    writeLogLine( QStringLiteral( "plugin" ),
                  QStringLiteral( "ListCloseWindow called for %1" ).arg( formatWindowHandle( listWin ) ) );

    invokeSafely( "ListCloseWindow", [ & ]() {
        closeViewer( listWin );
        return 0;
    },
                 0 );
}

__declspec( dllexport ) int __stdcall ListSearchText( HWND listWin, char* searchString, int searchParameter )
{
    using namespace klogg::tc::lister;
    initializeLogging();

    const QString query = toQString( searchString );
    writeLogLine( QStringLiteral( "plugin" ),
                  QStringLiteral( "ListSearchText called window=%1 query='%2' params=0x%3" )
                      .arg( formatWindowHandle( listWin ), query,
                            QString::number( searchParameter, 16 ) ) );

    return invokeSafely( "ListSearchText",
                         [ & ]() { return searchText( listWin, query, searchParameter ); }, kResultError );
}

__declspec( dllexport ) int __stdcall ListSearchTextW( HWND listWin, const wchar_t* searchString,
                                                      int searchParameter )
{
    using namespace klogg::tc::lister;
    initializeLogging();

    const QString query = toQString( searchString );
    writeLogLine( QStringLiteral( "plugin" ),
                  QStringLiteral( "ListSearchTextW called window=%1 query='%2' params=0x%3" )
                      .arg( formatWindowHandle( listWin ), query,
                            QString::number( searchParameter, 16 ) ) );

    return invokeSafely( "ListSearchTextW",
                         [ & ]() { return searchText( listWin, query, searchParameter ); }, kResultError );
}

__declspec( dllexport ) int __stdcall ListSendCommand( HWND listWin, int command, int parameter )
{
    using namespace klogg::tc::lister;
    initializeLogging();

    writeLogLine( QStringLiteral( "plugin" ),
                  QStringLiteral( "ListSendCommand called window=%1 command=%2 parameter=%3" )
                      .arg( formatWindowHandle( listWin ), QString::number( command ),
                            QString::number( parameter ) ) );

    return invokeSafely( "ListSendCommand",
                         [ & ]() { return sendCommand( listWin, command, parameter ); }, kResultError );
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

__declspec( dllexport ) int __stdcall ListGetDetectString( char* detectString, int maxLen )
{
    using namespace klogg::tc::lister;
    initializeLogging();

    writeLogLine( QStringLiteral( "plugin" ),
                  QStringLiteral( "ListGetDetectString called maxLen=%1" )
                      .arg( QString::number( maxLen ) ) );

    return invokeSafely( "ListGetDetectString",
                         [ & ]() { return populateDetectString( detectString, maxLen ); }, kResultError );
}

} // extern "C"
