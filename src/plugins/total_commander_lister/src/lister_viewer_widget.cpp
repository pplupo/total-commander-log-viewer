#include "lister_viewer_widget.h"

#include <QApplication>
#include <QDir>
#include <QMetaObject>
#include <QPointer>
#include <QVBoxLayout>

#include <exception>

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

    qInfo().noquote().nospace() << "[lister] attaching crawler to '"
                                << QDir::toNativeSeparators( filePath ) << "'";

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
        qInfo().noquote().nospace() << "[lister] crawler ready for '"
                                    << QDir::toNativeSeparators( currentFile_ ) << "'";
        return true;
    }
    catch ( const std::exception& ex ) {
        qCritical().noquote().nospace()
            << "[lister] failed to attach crawler to '"
            << QDir::toNativeSeparators( filePath ) << "': " << ex.what();
        currentCrawler_ = nullptr;
        currentFile_.clear();
        return false;
    }
    catch ( ... ) {
        qCritical().noquote().nospace()
            << "[lister] failed to attach crawler to '"
            << QDir::toNativeSeparators( filePath ) << "' due to unknown exception";
        currentCrawler_ = nullptr;
        currentFile_.clear();
        return false;
    }
}

bool ListerViewerWidget::loadFile( const QString& filePath )
{
    qInfo().noquote().nospace() << "[lister] loadFile '" << QDir::toNativeSeparators( filePath ) << "'";
    return attachCrawler( filePath );
}

bool ListerViewerWidget::loadNextFile( const QString& filePath )
{
    qInfo().noquote().nospace() << "[lister] loadNextFile '" << QDir::toNativeSeparators( filePath ) << "'";
    return attachCrawler( filePath );
}

void ListerViewerWidget::detachCrawler()
{
    if ( !currentCrawler_ ) {
        quickFindMux_->registerSelector( nullptr );
        qInfo().noquote().nospace() << "[lister] detachCrawler with no active view";
        return;
    }

    quickFindMux_->registerSelector( nullptr );

    qInfo().noquote().nospace() << "[lister] detaching crawler from '"
                                << QDir::toNativeSeparators( currentFile_ ) << "'";
    crawlerLayout_->removeWidget( currentCrawler_ );
    session_->close( currentCrawler_ );
    currentCrawler_->deleteLater();
    currentCrawler_ = nullptr;
    currentFile_.clear();
}

void ListerViewerWidget::closeFile()
{
    qInfo().noquote().nospace() << "[lister] closeFile";
    detachCrawler();
}

bool ListerViewerWidget::searchText( const QString& text, int searchParameters )
{
    if ( !currentCrawler_ ) {
        qWarning().noquote().nospace() << "[lister] searchText ignored - no active crawler";
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

    qInfo().noquote().nospace() << "[lister] searchText pattern='" << text << "' params=0x"
                                << QString::number( searchParameters, 16 );
    return true;
}

int ListerViewerWidget::sendCommand( int command, int parameter )
{
    Q_UNUSED( parameter );

    if ( !currentCrawler_ ) {
        qWarning().noquote().nospace() << "[lister] sendCommand " << command
                                       << " ignored - no active crawler";
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
        qInfo().noquote().nospace() << "[lister] sendCommand capabilities requested";
        return kCapabilitySupportsTextSearch | kCapabilityHandlesMultipleFiles
               | kCapabilitySupportsSelectAll | kCapabilitySupportsCopy;
    default:
        qInfo().noquote().nospace() << "[lister] sendCommand " << command << " not implemented";
        break;
    }

    return kResultNotImplemented;
}

void ListerViewerWidget::applyShowFlags( int showFlags )
{
    Q_UNUSED( showFlags );
}

} // namespace klogg::tc::lister
