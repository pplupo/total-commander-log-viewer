#include "lister_viewer_widget.h"

#include <QApplication>
#include <QPointer>
#include <QMetaObject>
#include <QVBoxLayout>

#include "crawlerwidget.h"
#include "quickfindmux.h"
#include "quickfindwidget.h"
#include "session.h"
#include "viewinterface.h"

namespace klogg::tc::lister {

ListerViewerWidget::ListerViewerWidget( QWidget* parent )
    : QWidget( parent )
    , session_( std::make_shared<Session>() )
    , quickFindMux_( std::make_unique<QuickFindMux>( session_->quickFindPattern() ) )
{
    setObjectName( QStringLiteral( "klogg_lister_viewer" ) );

    auto* layout = new QVBoxLayout( this );
    layout->setContentsMargins( 0, 0, 0, 0 );
    layout->setSpacing( 0 );

    crawlerContainer_ = new QWidget( this );
    crawlerLayout_ = new QVBoxLayout( crawlerContainer_ );
    crawlerLayout_->setContentsMargins( 0, 0, 0, 0 );
    crawlerLayout_->setSpacing( 0 );
    layout->addWidget( crawlerContainer_ );

    quickFindWidget_ = new QuickFindWidget( this );
    quickFindWidget_->hide();
    layout->addWidget( quickFindWidget_ );

    connect( quickFindWidget_, &QuickFindWidget::patternConfirmed, quickFindMux_.get(),
             &QuickFindMux::confirmPattern );
    connect( quickFindWidget_, &QuickFindWidget::patternUpdated, quickFindMux_.get(),
             &QuickFindMux::setNewPattern );
    connect( quickFindWidget_, &QuickFindWidget::cancelSearch, quickFindMux_.get(),
             &QuickFindMux::cancelSearch );
    connect( quickFindWidget_, &QuickFindWidget::searchForward, quickFindMux_.get(),
             &QuickFindMux::searchForward );
    connect( quickFindWidget_, &QuickFindWidget::searchBackward, quickFindMux_.get(),
             &QuickFindMux::searchBackward );
    connect( quickFindWidget_, &QuickFindWidget::searchNext, quickFindMux_.get(),
             &QuickFindMux::searchNext );

    connect( quickFindMux_.get(), &QuickFindMux::notify, quickFindWidget_, &QuickFindWidget::notify );
    connect( quickFindMux_.get(), &QuickFindMux::clearNotification, quickFindWidget_,
             &QuickFindWidget::clearNotification );
    connect( quickFindMux_.get(), &QuickFindMux::patternChanged, this,
             [this]( const QString& pattern ) {
                 quickFindWidget_->changeDisplayedPattern( pattern, true );
             } );

    setFocusPolicy( Qt::StrongFocus );
}

ListerViewerWidget::~ListerViewerWidget()
{
    detachCrawler();
}

void ListerViewerWidget::hookCrawlerSignals( CrawlerWidget* crawler )
{
    if ( !crawler ) {
        return;
    }

    quickFindMux_->registerSelector( crawler );

    connect( crawler, &CrawlerWidget::filteredViewChanged, this,
             [this, crawler]() {
                 if ( crawler == currentCrawler_ ) {
                     quickFindMux_->registerSelector( crawler );
                 }
             } );
}

bool ListerViewerWidget::attachCrawler( const QString& filePath )
{
    detachCrawler();

    try {
        ViewInterface* view = session_->open( filePath, [this]() -> ViewInterface* {
            auto* widget = new CrawlerWidget( crawlerContainer_ );
            widget->setParent( crawlerContainer_ );
            return widget;
        } );

        currentCrawler_ = static_cast<CrawlerWidget*>( view );
        crawlerLayout_->addWidget( currentCrawler_ );
        currentCrawler_->show();
        currentFile_ = filePath;

        hookCrawlerSignals( currentCrawler_ );
        return true;
    }
    catch ( ... ) {
        currentCrawler_ = nullptr;
        currentFile_.clear();
        return false;
    }
}

bool ListerViewerWidget::loadFile( const QString& filePath )
{
    return attachCrawler( filePath );
}

bool ListerViewerWidget::loadNextFile( const QString& filePath )
{
    return attachCrawler( filePath );
}

void ListerViewerWidget::detachCrawler()
{
    if ( !currentCrawler_ ) {
        quickFindMux_->registerSelector( nullptr );
        return;
    }

    quickFindMux_->registerSelector( nullptr );

    crawlerLayout_->removeWidget( currentCrawler_ );
    session_->close( currentCrawler_ );
    currentCrawler_->deleteLater();
    currentCrawler_ = nullptr;
    currentFile_.clear();
}

void ListerViewerWidget::closeFile()
{
    detachCrawler();
}

bool ListerViewerWidget::searchText( const QString& text, int searchParameters )
{
    if ( !currentCrawler_ ) {
        return false;
    }

    const bool matchCase = ( searchParameters & kSearchMatchCase ) != 0;
    const bool backwards = ( searchParameters & kSearchBackwards ) != 0;
    const bool regex = ( searchParameters & kSearchRegex ) != 0;

    quickFindMux_->setDirection( backwards ? QuickFindMux::Backward : QuickFindMux::Forward );

    if ( !text.isEmpty() ) {
        quickFindMux_->setNewPattern( text, !matchCase, regex );
        quickFindMux_->confirmPattern( text, !matchCase, regex );
    }

    if ( ( searchParameters & kSearchRepeat ) != 0 ) {
        quickFindMux_->searchNext();
    }
    else {
        quickFindMux_->searchNext();
    }

    return true;
}

int ListerViewerWidget::sendCommand( int command, int parameter )
{
    Q_UNUSED( parameter );

    if ( !currentCrawler_ ) {
        return kResultError;
    }

    switch ( command ) {
    case kCommandCopy: {
        auto* focusedWidget = QApplication::focusWidget();
        if ( focusedWidget ) {
            QMetaObject::invokeMethod( focusedWidget, "copy", Qt::DirectConnection );
            return kResultOk;
        }
        return kResultNotImplemented;
    }
    case kCommandSelectAll:
        currentCrawler_->selectAll();
        return kResultOk;
    case kCommandRefresh:
        currentCrawler_->reload();
        return kResultOk;
    case kCommandGetCapabilities:
        return kCapabilitySupportsTextSearch | kCapabilityHandlesMultipleFiles
               | kCapabilitySupportsSelectAll | kCapabilitySupportsCopy;
    default:
        break;
    }

    return kResultNotImplemented;
}

void ListerViewerWidget::applyShowFlags( int showFlags )
{
    Q_UNUSED( showFlags );
}

} // namespace klogg::tc::lister
