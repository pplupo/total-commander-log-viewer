#pragma once

#include <memory>

#include <QWidget>
#include <QString>

#include "lister_plugin_api.h"

class CrawlerWidget;
class QuickFindMux;
class QuickFindWidget;
class Session;
class QVBoxLayout;

namespace klogg::tc::lister {

class ListerViewerWidget : public QWidget {
    Q_OBJECT

  public:
    explicit ListerViewerWidget( QWidget* parent = nullptr );
    ~ListerViewerWidget() override;

    bool loadFile( const QString& filePath );
    bool loadNextFile( const QString& filePath );
    void closeFile();

    bool searchText( const QString& text, int searchParameters );
    int sendCommand( int command, int parameter );

    void applyShowFlags( int showFlags );

    CrawlerWidget* crawler() const
    {
        return currentCrawler_;
    }

  private:
    bool attachCrawler( const QString& filePath );
    void detachCrawler();
    void hookCrawlerSignals( CrawlerWidget* crawler );

    std::shared_ptr<Session> session_;
    std::unique_ptr<QuickFindMux> quickFindMux_;
    QuickFindWidget* quickFindWidget_ = nullptr;
    QWidget* crawlerContainer_ = nullptr;
    QVBoxLayout* crawlerLayout_ = nullptr;
    CrawlerWidget* currentCrawler_ = nullptr;
    QString currentFile_;
};

} // namespace klogg::tc::lister
