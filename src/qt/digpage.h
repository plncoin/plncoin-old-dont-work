#ifndef DIGPAGE_H
#define DIGPAGE_H

/*
#include <QDialog>
#include <QThread>
#include <QTimer>

#include "configfile.h"

namespace Ui {
    class DigPage;
}

class ClientModel;
class WalletModel;

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

class HashExecutor : public QObject
{
    Q_OBJECT

public slots:
    void request(const QString &command);

signals:
    void reply(int category, const QString &command);
};

const static int TIMER_INTERVAL = 1000;

class DigPage : public QDialog
{
    Q_OBJECT

    public:
        explicit DigPage(QWidget *parent = 0);
        ~DigPage();

        void setClientModel(ClientModel *clientModel);
        void setWalletModel(WalletModel *walletModel);
        void showOutOfSyncWarning(bool fShow);

        void setHashes(const qint64 hashes);
        const qint64 getHashes();

        void setBalance(const qint64 balance);
        const qint64 getBalance();

        void setUnconfirmed(const qint64 unconfirmed);
        const qint64 getUnconfirmed();

        void setImmature(const qint64 immature);
        const qint64 getImmature();

        enum MessageClass {
            MC_ERROR,
            MC_DEBUG,
            CMD_REQUEST,
            CMD_REPLY,
            CMD_ERROR
        };

    signals:
        void stopExecutor();
        void cmdRequest(const QString &command);

    public slots:
        void doDig1();

        void setBalance(qint64 balance, qint64 unconfirmedBalance, qint64 immatureBalance);
        void message(int category, const QString &message, bool html = false);

    private slots:
        void updateExecutor();
        void updateDisplayUnit();
        //void updateAlerts(const QString &warnings);

    private:
        void startExecutor();

        Ui::DigPage *ui;
        ClientModel *clientModel;
        WalletModel *walletModel;

        ConfigFile config;
        QString url, user, password;
        qint64 hashes, balance, unconfirmed, immature;
        bool digging, digStarted;
};

*/

#endif // DIGPAGE_H
