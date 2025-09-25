#include <windows.h>

#include <memory>
#include <mutex>
#include <unordered_map>

#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QString>
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
    auto& st = state();
    if ( !QCoreApplication::instance() ) {
        int argc = 0;
        static char appName[] = "klogg_lister";
        static char* argv[] = { appName, nullptr };
        st.app = std::make_unique<QApplication>( argc, argv );
    }

    if ( !st.settingsInitialized ) {
        st.settingsInitialized = true;
        PersistentInfo::overrideApplicationKeys( QStringLiteral( "klogg_lister" ),
                                                 QStringLiteral( "klogg_lister_session" ) );
        PersistentInfo::overridePortableMode( true );
        Configuration::getSynced();
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

    auto viewer = std::make_unique<ListerViewerWidget>();
    viewer->setAttribute( Qt::WA_NativeWindow );
    viewer->setWindowFlag( Qt::FramelessWindowHint );
    viewer->applyShowFlags( showFlags );

    if ( !viewer->loadFile( filePath ) ) {
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
