#include "digpage.h"
#include "ui_digpage.h"

/*
#include "clientmodel.h"
#include "walletmodel.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"
#include "guiutil.h"
#include "bitcoinrpc.h"

#include <QThread>
#include <QTimer>
#include <QtScript/QScriptEngine>

bool parseCommandLineHash(std::vector<std::string> &args, const std::string &strCommand)
{
    enum CmdParseState
    {
        STATE_EATING_SPACES,
        STATE_ARGUMENT,
        STATE_SINGLEQUOTED,
        STATE_DOUBLEQUOTED,
        STATE_ESCAPE_OUTER,
        STATE_ESCAPE_DOUBLEQUOTED
    } state = STATE_EATING_SPACES;
    std::string curarg;
    foreach(char ch, strCommand)
    {
        switch(state)
        {
        case STATE_ARGUMENT: // In or after argument
        case STATE_EATING_SPACES: // Handle runs of whitespace
            switch(ch)
            {
            case '"': state = STATE_DOUBLEQUOTED; break;
            case '\'': state = STATE_SINGLEQUOTED; break;
            case '\\': state = STATE_ESCAPE_OUTER; break;
            case ' ': case '\n': case '\t':
                if(state == STATE_ARGUMENT) // Space ends argument
                {
                    args.push_back(curarg);
                    curarg.clear();
                }
                state = STATE_EATING_SPACES;
                break;
            default: curarg += ch; state = STATE_ARGUMENT;
            }
            break;
        case STATE_SINGLEQUOTED: // Single-quoted string
            switch(ch)
            {
            case '\'': state = STATE_ARGUMENT; break;
            default: curarg += ch;
            }
            break;
        case STATE_DOUBLEQUOTED: // Double-quoted string
            switch(ch)
            {
            case '"': state = STATE_ARGUMENT; break;
            case '\\': state = STATE_ESCAPE_DOUBLEQUOTED; break;
            default: curarg += ch;
            }
            break;
        case STATE_ESCAPE_OUTER: // '\' outside quotes
            curarg += ch; state = STATE_ARGUMENT;
            break;
        case STATE_ESCAPE_DOUBLEQUOTED: // '\' in double-quoted text
            if(ch != '"' && ch != '\\') curarg += '\\'; // keep '\' for everything but the quote and '\' itself
            curarg += ch; state = STATE_DOUBLEQUOTED;
            break;
        }
    }
    switch(state) // final state
    {
    case STATE_EATING_SPACES:
        return true;
    case STATE_ARGUMENT:
        args.push_back(curarg);
        return true;
    default: // ERROR to end in one of the other states
        return false;
    }
}

void HashExecutor::request(const QString &command)
{
    std::vector<std::string> args;
    if(!parseCommandLineHash(args, command.toStdString()))
    {
        emit reply(DigPage::CMD_ERROR, QString("Parse error: unbalanced ' or \""));
        return;
    }
    if(args.empty())
        return; // Nothing to do
    try
    {
        std::string strPrint;
        // Convert argument list to JSON objects in method-dependent way,
        // and pass it along with the method name to the dispatcher.
        json_spirit::Value result = tableRPC.execute(
            args[0],
            RPCConvertValues(args[0], std::vector<std::string>(args.begin() + 1, args.end())));

        // Format result reply
        if (result.type() == json_spirit::null_type)
            strPrint = "";
        else if (result.type() == json_spirit::str_type)
            strPrint = result.get_str();
        else
            strPrint = write_string(result, true);

        emit reply(DigPage::CMD_REPLY, QString::fromStdString(strPrint));
    }
    catch (json_spirit::Object& objError)
    {
        try // Nice formatting for standard-format error
        {
            int code = find_value(objError, "code").get_int();
            std::string message = find_value(objError, "message").get_str();
            emit reply(DigPage::CMD_ERROR, QString::fromStdString(message) + " (code " + QString::number(code) + ")");
        }
        catch(std::runtime_error &) // raised when converting to invalid type, i.e. missing code or message
        {   // Show raw JSON object
            emit reply(DigPage::CMD_ERROR, QString::fromStdString(write_string(json_spirit::Value(objError), false)));
        }
    }
    catch (std::exception& e)
    {
        emit reply(DigPage::CMD_ERROR, QString("Error: ") + QString::fromStdString(e.what()));
    }
}

DigPage::DigPage(QWidget *parent) : QDialog(parent), ui(new Ui::DigPage), clientModel(0), walletModel(0), hashes(0), balance(-1), unconfirmed(-1), immature(-1), digging(false), digStarted(false), url(""), user(""), password("")
{
    ui->setupUi(this);

    connect(ui->btnDig, SIGNAL(clicked()), this, SLOT(doDig1()));

    // Ustawienie statusu synchronizacji
    ui->labelWalletStatus->setText("(" + tr("out of sync") + ")");
    showOutOfSyncWarning(true);

    startExecutor();
}

DigPage::~DigPage()
{
    emit stopExecutor();

    delete ui;
}

void DigPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
        /*
        connect(model, SIGNAL(alertsChanged(QString)), this, SLOT(updateAlerts(QString)));
        updateAlerts(model->getStatusBarWarnings());
        *
    }
}

void DigPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        // Ustawienie danych z portfela
        setBalance(model->getBalance(), model->getUnconfirmedBalance(), model->getImmatureBalance());
        connect(model, SIGNAL(balanceChanged(qint64, qint64, qint64)), this, SLOT(setBalance(qint64, qint64, qint64)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
    }

    // Aktualizacja jednostek
    updateDisplayUnit();

    // Ustawienie aktualnej liczby Hash'y
    this->setHashes(config.getMaxHashes());
}

void DigPage::startExecutor()
{
    QThread *thread = new QThread();
    HashExecutor *executor = new HashExecutor();
    executor->moveToThread(thread);

    // Replies from executor object must go to this object
    connect(executor, SIGNAL(reply(int,QString)), this, SLOT(message(int,QString)));
    // Requests from this object must go to executor
    connect(this, SIGNAL(cmdRequest(QString)), executor, SLOT(request(QString)));

    // On stopExecutor signal
    // - queue executor for deletion (in execution thread)
    // - quit the Qt event loop in the execution thread
    connect(this, SIGNAL(stopExecutor()), executor, SLOT(deleteLater()));
    connect(this, SIGNAL(stopExecutor()), thread, SLOT(quit()));
    // Queue the thread for deletion (in this thread) when it is finished
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));

    // Default implementation of QThread::run() simply spins up an event loop in the thread,
    // which is what we want.
    thread->start();

    // Aktualizacja liczby hash'y
    QThread *updateThread = new QThread(this);
    QTimer *timer = new QTimer(0);

    timer->moveToThread(updateThread);
    timer->setInterval(TIMER_INTERVAL);

    connect(updateThread, SIGNAL(started()), timer, SLOT(start()));
    connect(timer, SIGNAL(timeout()), this, SLOT(updateExecutor()));
    connect(this, SIGNAL(stopExecutor()), timer, SLOT(deleteLater()));
    connect(this, SIGNAL(stopExecutor()), updateThread, SLOT(quit()));
    connect(updateThread, SIGNAL(finished()), updateThread, SLOT(deleteLater()));

    updateThread->start();
}

void DigPage::updateExecutor()
{
    cmdRequest("getmininginfo");
}

void DigPage::message(int category, const QString &message, bool html)
{
    QScriptEngine engine;
    QScriptValue sv = engine.evaluate("(" + message + ")");

    int hashes = 0;
    if(sv.property("hashespersec").isNumber()) {
        hashes = sv.property("hashespersec").toInt32();
    }

    this->showOutOfSyncWarning(hashes < 1);
    this->setHashes(hashes);
}

void DigPage::showOutOfSyncWarning(bool fShow)
{
    if(!digStarted) {
        digStarted = !fShow;
    }

    ui->labelWalletStatus->setVisible(fShow && !digStarted);
}

void DigPage::updateDisplayUnit()
{
    if(walletModel && walletModel->getOptionsModel())
    {
        if(balance != -1) {
            setBalance(balance, unconfirmed, immature);
        }

        /*
        txdelegate->unit = walletModel->getOptionsModel()->getDisplayUnit();
        ui->listTransactions->update();
        *
    }
}

void DigPage::setBalance(qint64 balance, qint64 unconfirmedBalance, qint64 immatureBalance)
{
    int unit = walletModel->getOptionsModel()->getDisplayUnit();

    this->balance = balance;
    this->unconfirmed = unconfirmedBalance;
    this->immature = immatureBalance;

    ui->lblBalance->setText(BitcoinUnits::formatWithUnit(unit, balance));
    ui->lblUnconfirmed->setText(BitcoinUnits::formatWithUnit(unit, unconfirmedBalance));
    ui->lblInmature->setText(BitcoinUnits::formatWithUnit(unit, immatureBalance));
}

/*
void DigPage::updateAlerts(const QString &warnings)
{
}
*

// Rozpoczęcie kopania
void DigPage::doDig1()
{
    if(digging)
    {
        cmdRequest("setgenerate false 0");
        digging = false;

        ui->btnDig->setText(tr("Resume digging"));
    }
    else
    {
        /*
        cmdRequest("addnode 193.187.64.12 add");
        cmdRequest("addnode 193.187.64.7 add");
        *
        cmdRequest("setgenerate true 16");
        digging = true;

        ui->btnDig->setText(tr("Stop digging"));

        if(!digStarted) {
            ui->labelWalletStatus->setText("(" + tr("synchronizacja") + "...)");
        }
    }
}

void DigPage::setHashes(const qint64 hashes)
{
    this->hashes = hashes;
    ui->lblHashes->setText(QString::number(hashes));

    // Zapisanie liczby hash'y w konfiguracji
    config.setMaxHashes(hashes);

    // Aktualna liczba hash'y
    ui->progressBar->setMaximum(config.getMaxHashes());
    ui->progressBar->setValue(hashes);
    ui->progressBar->setStatusTip("Max: " + QString::number(config.getMaxHashes()));
}

const qint64 DigPage::getHashes()
{
    return this->hashes;
}

void DigPage::setBalance(const qint64 balance)
{
    this->setBalance(balance, this->unconfirmed, this->immature);
}

const qint64 DigPage::getBalance()
{
    return this->balance;
}

void DigPage::setUnconfirmed(const qint64 unconfirmed)
{
    this->setBalance(this->balance, unconfirmed, this->immature);
}

const qint64 DigPage::getUnconfirmed()
{
    return this->unconfirmed;
}

void DigPage::setImmature(const qint64 immature)
{
    this->setBalance(this->balance, this->unconfirmed, immature);
}

const qint64 DigPage::getImmature()
{
    return this->immature;
}
*/
