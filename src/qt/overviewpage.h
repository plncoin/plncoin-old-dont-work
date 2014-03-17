#ifndef OVERVIEWPAGE_H
#define OVERVIEWPAGE_H

#include <QWidget>
#include <QThread>
#include <QTimer>
#include <QFile>
#include <QVector>

#include "configfile.h"
#include "update.h"

namespace Ui {
    class OverviewPage;
}
class ClientModel;
class WalletModel;
class TxViewDelegate;
class TransactionFilterProxy;

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

const static int TIMER_INTERVAL = 1000;

class HashExecutor : public QObject
{
    Q_OBJECT

public slots:
    void request(const QString &command);

signals:
    void reply(int category, const QString &command);
};

/** Overview ("home") page widget */
class OverviewPage : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewPage(QWidget *parent = 0);
    ~OverviewPage();

    void setClientModel(ClientModel *clientModel);
    void setWalletModel(WalletModel *walletModel);
    void showOutOfSyncWarning(bool fShow);
    void showOutOfSyncWarning2(bool fShow);

    void setHashes(const qint64 hashes);
    const qint64 getHashes();

    enum MessageClass {
        MC_ERROR,
        MC_DEBUG,
        CMD_REQUEST,
        CMD_REPLY,
        CMD_ERROR
    };

public slots:
    void setBalance(qint64 balance, qint64 unconfirmedBalance, qint64 immatureBalance);
    void doDig();
    void message(int category, const QString &message, bool html = false);
    void showDownloadDialog(bool isLatest);
    void showDownloadFinishedDialog();

signals:
    void transactionClicked(const QModelIndex &index);
    void stopExecutor();
    void cmdRequest(const QString &command);

private:
    void startExecutor();

    Ui::OverviewPage *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;
    qint64 currentBalance;
    qint64 currentUnconfirmedBalance;
    qint64 currentImmatureBalance;

    TxViewDelegate *txdelegate;
    TransactionFilterProxy *filter;
    UpdateManager updateManager;

    ConfigFile config;
    qint64 hashes;
    bool digging, digStarted;
    QVector<double> chartX, chartY;
    int refresh, hTime, current, maxRange;

private slots:
    void updateDisplayUnit();
    void handleTransactionClicked(const QModelIndex &index);
    void updateAlerts(const QString &warnings);
    void updateExecutor();
};

#endif // OVERVIEWPAGE_H
