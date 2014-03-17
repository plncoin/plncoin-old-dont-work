#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "clientmodel.h"
#include "walletmodel.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"
#include "transactiontablemodel.h"
#include "transactionfilterproxy.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "bitcoinrpc.h"

#include <QAbstractItemDelegate>
#include <QPainter>

#include <QThread>
#include <QTimer>
#include <QtScript/QScriptEngine>
#include <QMessageBox>
#include <QFileDialog>

#define DECORATION_SIZE 64
#define NUM_ITEMS 3

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
        emit reply(OverviewPage::CMD_ERROR, QString("Parse error: unbalanced ' or \""));
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

        emit reply(OverviewPage::CMD_REPLY, QString::fromStdString(strPrint));
    }
    catch (json_spirit::Object& objError)
    {
        try // Nice formatting for standard-format error
        {
            int code = find_value(objError, "code").get_int();
            std::string message = find_value(objError, "message").get_str();
            emit reply(OverviewPage::CMD_ERROR, QString::fromStdString(message) + " (code " + QString::number(code) + ")");
        }
        catch(std::runtime_error &) // raised when converting to invalid type, i.e. missing code or message
        {   // Show raw JSON object
            emit reply(OverviewPage::CMD_ERROR, QString::fromStdString(write_string(json_spirit::Value(objError), false)));
        }
    }
    catch (std::exception& e)
    {
        emit reply(OverviewPage::CMD_ERROR, QString("Error: ") + QString::fromStdString(e.what()));
    }
}

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    TxViewDelegate(): QAbstractItemDelegate(), unit(BitcoinUnits::BTC)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect mainRect = option.rect;
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(value.canConvert<QBrush>())
        {
            QBrush brush = qvariant_cast<QBrush>(value);
            foreground = brush.color();
        }

        painter->setPen(foreground);
        painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, address);

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setPen(foreground);
        QString amountText = BitcoinUnits::formatWithUnit(unit, amount, true);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);

        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(amountRect, Qt::AlignLeft|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;

};
#include "overviewpage.moc"

OverviewPage::OverviewPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    clientModel(0),
    walletModel(0),
    currentBalance(-1),
    currentUnconfirmedBalance(-1),
    currentImmatureBalance(-1),
    txdelegate(new TxViewDelegate()),
    filter(0),
    hashes(0),
    digging(false),
    digStarted(false),
    updateManager(parent),
    chartX(180),
    chartY(180),
    refresh(0),
    current(0),
    hTime(0),
    maxRange(0)
{
    ui->setupUi(this);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));
    connect(ui->btnDig, SIGNAL(clicked()), this, SLOT(doDig()));

    // init "out of sync" warning labels
    ui->labelWalletStatus->setText("(" + tr("out of sync") + ")");
    ui->labelTransactionsStatus->setText("(" + tr("out of sync") + ")");

    // start with displaying the "out of sync" warnings
    ui->labelDigStatus->setText("(" + tr("out of sync") + ")");
    showOutOfSyncWarning(true);

    ui->labelMining->setText(tr("Mining"));

    updateManager.checkUpdate();
    connect(&updateManager, SIGNAL(latest(bool)), this, SLOT(showDownloadDialog(bool)));
    connect(&updateManager, SIGNAL(downloaded()), this, SLOT(showDownloadFinishedDialog()));

    // Wyzerownaie Hash'y
    this->setHashes(0);

    for(int i = 0; i < chartX.size(); ++i) {
        chartX[i] = (double)i / 6;
        chartY[i] = (double)0;
    }

    ui->qChart->addGraph();
    ui->qChart->graph(0)->setData(chartX, chartY);
    ui->qChart->xAxis->setRange(30, 0);
    ui->qChart->yAxis->setRange(0, 10000);
    ui->qChart->xAxis->grid()->setVisible(false);
    ui->qChart->xAxis->setTickLabels(false);
    ui->qChart->setBackground(QWidget::palette().color(QWidget::backgroundRole()));
    ui->qChart->replot();

    current = chartY.size();

    startExecutor();
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    if(filter)
        emit transactionClicked(filter->mapToSource(index));
}

OverviewPage::~OverviewPage()
{
    this->setHashes(0);
    emit stopExecutor();
    delete ui;
}

// Wyświetla dialog pobierania aktualizacji
void OverviewPage::showDownloadDialog(bool isLatest)
{
    if(!isLatest)
    {
        int result = QMessageBox(QMessageBox::Information, tr("PLNcoin-Qt Update"), tr("A new patch for PLNcoin-Qt is available. Do you wanna download it?"), QMessageBox::Yes | QMessageBox::No).exec();
        if(result == QMessageBox::Yes)
        {
            QString filename = QFileDialog::getSaveFileName(this, "Save file", "plncoin-qt", "All Files");
            if(!filename.isEmpty()) {
                updateManager.downloadAndSave(filename);
            }
        }
        else {
            qApp->exit();
        }
    }
}

// Wyświetla dialog o zakończeniu pobierania aktualizacji
void OverviewPage::showDownloadFinishedDialog()
{
    printf("App: %s", qApp->arguments()[0].toStdString().c_str());
    QMessageBox(QMessageBox::Information, "PLNcoin-Qt", "Aktualizacja została poprawnie pobrana. Uruchom nową wersję aplikacji.", QMessageBox::Ok).exec();
}

void OverviewPage::setBalance(qint64 balance, qint64 unconfirmedBalance, qint64 immatureBalance)
{
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    currentBalance = balance;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;
    ui->labelBalance->setText(BitcoinUnits::formatWithUnit(unit, balance));
    ui->labelUnconfirmed->setText(BitcoinUnits::formatWithUnit(unit, unconfirmedBalance));
    ui->labelImmature->setText(BitcoinUnits::formatWithUnit(unit, immatureBalance));

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = immatureBalance != 0;
    ui->labelImmature->setVisible(showImmature);
    ui->labelImmatureText->setVisible(showImmature);
}

void OverviewPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
        // Show warning if this is a prerelease version
        connect(model, SIGNAL(alertsChanged(QString)), this, SLOT(updateAlerts(QString)));
        updateAlerts(model->getStatusBarWarnings());
    }
}

void OverviewPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter = new TransactionFilterProxy();
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->sort(TransactionTableModel::Status, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter);
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getUnconfirmedBalance(), model->getImmatureBalance());
        connect(model, SIGNAL(balanceChanged(qint64, qint64, qint64)), this, SLOT(setBalance(qint64, qint64, qint64)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
    }

    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();

    // Ustawienie aktualnej liczby Hash'y
    this->setHashes(config.getMaxHashes());
}

void OverviewPage::startExecutor()
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
    timer->setInterval(TIMER_INTERVAL);

    timer->moveToThread(updateThread);

    connect(updateThread, SIGNAL(started()), timer, SLOT(start()));
    connect(timer, SIGNAL(timeout()), this, SLOT(updateExecutor()));
    connect(this, SIGNAL(stopExecutor()), timer, SLOT(deleteLater()));
    connect(this, SIGNAL(stopExecutor()), updateThread, SLOT(quit()));
    connect(updateThread, SIGNAL(finished()), updateThread, SLOT(deleteLater()));

    updateThread->start();
}

void OverviewPage::updateExecutor()
{
    cmdRequest("getmininginfo");
}

void OverviewPage::message(int category, const QString &message, bool html)
{
    QScriptEngine engine;
    QScriptValue sv = engine.evaluate("(" + message + ")");

    int hashes = 0;
    if(sv.property("hashespersec").isNumber()) {
        hashes = sv.property("hashespersec").toInt32();
    }

    this->showOutOfSyncWarning2(hashes < 1);
    this->setHashes(hashes);
}

void OverviewPage::showOutOfSyncWarning2(bool fShow)
{
    if(!digStarted) {
        digStarted = !fShow;
    }

    ui->labelDigStatus->setVisible(fShow && !digStarted);
}

void OverviewPage::updateDisplayUnit()
{
    if(walletModel && walletModel->getOptionsModel())
    {
        if(currentBalance != -1)
            setBalance(currentBalance, currentUnconfirmedBalance, currentImmatureBalance);

        // Update txdelegate->unit with the current unit
        txdelegate->unit = walletModel->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();
    }
}

void OverviewPage::updateAlerts(const QString &warnings)
{
    this->ui->labelAlerts->setVisible(!warnings.isEmpty());
    this->ui->labelAlerts->setText(warnings);
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);

    this->showOutOfSyncWarning2(fShow && !digStarted);
}


// Rozpoczęcie kopania
void OverviewPage::doDig()
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
        */
        cmdRequest("setgenerate true 16");
        digging = true;

        ui->btnDig->setText(tr("Stop digging"));

        if(!digStarted) {
            ui->labelDigStatus->setText("(" + tr("synchronizacja") + "...)");
        }
    }
}

void OverviewPage::setHashes(const qint64 hashes)
{
    this->hashes = hashes;
    ui->lblHashes->setText(QString::number(digging ? hashes : 0));

    // Zapisanie liczby hash'y w konfiguracji
    config.setMaxHashes(hashes);

    // Aktualna liczba hash'y
    ui->progressBar->setMaximum(config.getMaxHashes());
    ui->progressBar->setValue(digging ? hashes : 0);
    ui->progressBar->setStatusTip("Max: " + QString::number(config.getMaxHashes()));

    // Aktualizacja wykresu
    if((hashes > 0 && digStarted) || hTime > 0)
    {
        if(!(hTime++ % 10))
        {
            // Ustawienie max liczby na osi y
            if(hashes > maxRange)
            {
                maxRange = hashes + 1000;
                ui->qChart->yAxis->setRange(0, maxRange);
            }

            // Przepisanie poprzednich wartości
            for(int i = 0; i < chartY.size() - 1; ++i) {
                chartY[i] = chartY[i + 1];
            }

            chartY[chartY.size() - 1] = (double)hashes;

            ui->qChart->graph(0)->setData(chartX, chartY);
            ui->qChart->replot();

            // Uniknięcie przekroczenia zakresu int
            if(hTime > 10) hTime-= 10;
        }
    }
}

const qint64 OverviewPage::getHashes()
{
    return this->hashes;
}
