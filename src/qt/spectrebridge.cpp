#include "spectrebridge.h"

#include "spectregui.h"
#include "guiutil.h"

#ifndef Q_MOC_RUN
#include <boost/algorithm/string.hpp>
#endif

#include "editaddressdialog.h"

#include "transactiontablemodel.h"
#include "transactionrecord.h"
#include "transactiondesc.h"
#include "addresstablemodel.h"

#include "messagemodel.h"

#include "clientmodel.h"
#include "walletmodel.h"
#include "messagemodel.h"
#include "optionsmodel.h"

#include "bitcoinunits.h"
#include "coincontrol.h"
#include "coincontroldialog.h"
#include "ringsig.h"

#include "askpassphrasedialog.h"

#include "txdb.h"
#include "state.h"

#include "extkey.h"

#include "bridgetranslations.h"

#include <QApplication>
#include <QThread>
#include <QClipboard>
#include <QMessageBox>
#include <QSortFilterProxyModel>

#include <QVariantList>
#include <QVariantMap>
#include <QDir>
#include <QtGui/qtextdocument.h>
#include <list>
#define ROWS_TO_REFRESH 200

extern CWallet* pwalletMain;
extern CBlockIndex* pindexBest;
extern CBlockIndex* FindBlockByHeight(int nHeight);
extern CBlockIndex *InsertBlockIndex(uint256 hash);
extern double GetDifficulty(const CBlockIndex* blockindex);

TransactionModel::TransactionModel(QObject *parent) :
        QObject(parent),
        ttm(new QSortFilterProxyModel()),
        running(false)
{ }

TransactionModel::~TransactionModel()
{
  delete ttm;
}

void TransactionModel::init(ClientModel * clientModel, TransactionTableModel * transactionTableModel)
{
    this->clientModel = clientModel;

    ttm->setSourceModel(transactionTableModel);
    ttm->setSortRole(Qt::EditRole);
    ttm->sort(TransactionTableModel::Status, Qt::DescendingOrder);
}

QVariantMap TransactionModel::addTransaction(int row)
{
    QModelIndex status   = ttm->index    (row, TransactionTableModel::Status);
    QModelIndex date     = status.sibling(row, TransactionTableModel::Date);
    QModelIndex address  = status.sibling(row, TransactionTableModel::ToAddress);
    QModelIndex amount   = status.sibling(row, TransactionTableModel::Amount);

    QVariantMap transaction;

    transaction.insert("id",   status.data(TransactionTableModel::TxIDRole).toString());
    transaction.insert("tt",   status.data(Qt::ToolTipRole).toString());
    transaction.insert("c",    status.data(TransactionTableModel::ConfirmationsRole).toLongLong());
    transaction.insert("s",    status.data(Qt::DecorationRole).toString());
    transaction.insert("d",    date.data(Qt::EditRole).toInt());
    transaction.insert("d_s",  date.data().toString());
    transaction.insert("t",    TransactionRecord::getTypeShort(status.data(TransactionTableModel::TypeRole).toInt()));
    transaction.insert("t_l",  status.sibling(row, TransactionTableModel::Type).data().toString());
    transaction.insert("ad_c", address.data(Qt::ForegroundRole).value<QColor>().name());
    transaction.insert("ad",   address.data(TransactionTableModel::AddressRole).toString());
    transaction.insert("ad_l", address.data(TransactionTableModel::LabelRole).toString());
    transaction.insert("ad_d", address.data().toString());
    transaction.insert("n",    status.sibling(row, TransactionTableModel::Narration).data().toString());
    transaction.insert("am_c", amount.data(Qt::ForegroundRole).value<QColor>().name());
    transaction.insert("am",   amount.data(TransactionTableModel::AmountRole).toString());
    transaction.insert("am_d", amount.data().toString());

    return transaction;
}

void TransactionModel::populateRows(int start, int end)
{
    if(start > ROWS_TO_REFRESH)
        return;

    if(!prepare())
        return;

    if (end > ROWS_TO_REFRESH)
        end = ROWS_TO_REFRESH;

    QVariantList transactions;

    while(start <= end)
    {
        if(visibleTransactions.first() == "*"||visibleTransactions.contains(ttm->index(start, TransactionTableModel::Type).data().toString()))
            transactions.append(addTransaction(start));

        start++;
    }
    if(!transactions.isEmpty())
        emitTransactions(transactions);

    running = false;
}

void TransactionModel::populatePage()
{

    if(!prepare())
        return;

    QVariantList transactions;

    int row = -1;

    while(++row < numRows && ttm->index(row, 0).isValid())
        if(visibleTransactions.first() == "*"||visibleTransactions.contains(ttm->index(row, TransactionTableModel::Type).data().toString()))
            transactions.append(addTransaction(row));

    if(!transactions.isEmpty())
        emitTransactions(transactions);

    running = false;

}

QSortFilterProxyModel * TransactionModel::getModel()
{
    return ttm;
}

bool TransactionModel::isRunning() {
    return running;
}

bool TransactionModel::prepare()
{
    if (this->running)
        return false;

    numRows = ttm->rowCount();
    ttm->sort(TransactionTableModel::Status, Qt::DescendingOrder);
    rowsPerPage = clientModel->getOptionsModel()->getRowsPerPage();
    visibleTransactions = clientModel->getOptionsModel()->getVisibleTransactions();

    this->running = true;

    return true;
}


QVariantMap AddressModel::addAddress(int row)
{
    QVariantMap address;
    QModelIndex label = atm->index(row, AddressTableModel::Label);

    address.insert("type",        label.data(AddressTableModel::TypeRole).toString());
    address.insert("label_value", label.data(Qt::EditRole).toString());
    address.insert("label",       label.data().toString());
    address.insert("address",     label.sibling(row, AddressTableModel::Address).data().toString());
    address.insert("pubkey",      label.sibling(row, AddressTableModel::Pubkey).data().toString());
    address.insert("at",          label.sibling(row, AddressTableModel::AddressType).data().toString());

    return address;
}

void AddressModel::poplateRows(int start, int end)
{
    QVariantList addresses;

    while(start <= end)
    {
        if(!atm->index(start, 0).isValid())
            continue;

        addresses.append(addAddress(start++));
    }
    emitAddresses(addresses);
}

void AddressModel::populateAddressTable()
{
    running = true;

    int row = -1;
    int end = atm->rowCount();
    QVariantList addresses;

    while(++row < end)
    {
        if(!atm->index(row, 0).isValid())
            continue;

        addresses.append(addAddress(row));
    }

    emitAddresses(addresses, true);

    running = false;
}

bool AddressModel::isRunning() {
    return running;
}


QString MessageThread::addMessage(int row)
{
    return QString("{\"id\":\"%10\",\"type\":\"%1\",\"sent_date\":\"%2\",\"received_date\":\"%3\", \"label_value\":\"%4\",\"label\":\"%5\",\"labelTo\":\"%11\",\"to_address\":\"%6\",\"from_address\":\"%7\",\"message\":\"%8\",\"read\":%9},")
            .arg(mtm->index(row, MessageModel::Type)            .data().toString())
            .arg(QString::number(mtm->index(row, MessageModel::SentDateTime)    .data().toDateTime().toTime_t()).toHtmlEscaped())
            .arg(QString::number(mtm->index(row, MessageModel::ReceivedDateTime).data().toDateTime().toTime_t()).toHtmlEscaped())
            .arg(mtm->index(row, MessageModel::Label)           .data(MessageModel::LabelRole).toString())
            .arg(mtm->index(row, MessageModel::Label)           .data().toString().replace("\\", "\\\\").replace("/", "\\/").replace("\"","\\\""))
            .arg(mtm->index(row, MessageModel::ToAddress)       .data().toString())
            .arg(mtm->index(row, MessageModel::FromAddress)     .data().toString())
            .arg(mtm->index(row, MessageModel::Message).data().toString().toHtmlEscaped().replace("\\", "\\\\").replace("\"","\\\"").replace("\n", "\\n"))
            .arg(mtm->index(row, MessageModel::Read)            .data().toBool())
            .arg(mtm->index(row, MessageModel::Key)             .data().toString())
            .arg(mtm->index(row, MessageModel::LabelTo)         .data().toString().replace("\\", "\\\\").replace("/", "\\/").replace("\"","\\\""));
}

void MessageThread::run()
{
    int row = -1;
    QString messages;
    while (mtm->index(++row, 0, QModelIndex()).isValid())
        messages.append(addMessage(row));

    emitMessages(messages, true);
}


SpectreBridge::SpectreBridge(SpectreGUI *window, QObject *parent) :
    QObject         (parent),
    window          (window),
    transactionModel(new TransactionModel()),
    addressModel    (new AddressModel()),
    thMessage       (new MessageThread()),
    info            (new QVariantMap()),
    async           (new QThread())
{
    async->start();
    connect(transactionModel->getModel(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), SLOT(updateTransactions(QModelIndex,QModelIndex)));
    connect(transactionModel->getModel(), SIGNAL(rowsInserted(QModelIndex,int,int)),    SLOT(insertTransactions(QModelIndex,int,int)));
}

SpectreBridge::~SpectreBridge()
{
    delete transactionModel;
    delete addressModel;
    delete thMessage;
    delete async;
    delete info;
}

// This is just a hook, we won't really be setting the model...
void SpectreBridge::setClientModel()
{
    info->insert("version", CLIENT_VERSION);
    info->insert("build",   window->clientModel->formatFullVersion());
    info->insert("date",    window->clientModel->formatBuildDate());
    info->insert("name",    window->clientModel->clientName());

    populateOptions();

    emit infoChanged();
}

void SpectreBridge::setWalletModel() {
    connect(window->clientModel->getOptionsModel(), SIGNAL(visibleTransactionsChanged(QStringList)), SLOT(populateTransactionTable()));
}

void SpectreBridge::jsReady() {
    window->walletModel->getOptionsModel()->emitDisplayUnitChanged(window->walletModel->getOptionsModel()->getDisplayUnit());
    window->walletModel->getOptionsModel()->emitReserveBalanceChanged(window->walletModel->getOptionsModel()->getReserveBalance());
    window->walletModel->getOptionsModel()->emitRowsPerPageChanged(window->walletModel->getOptionsModel()->getRowsPerPage());
    window->setNumConnections(window->clientModel->getNumConnections());
    window->setNumBlocks(window->clientModel->getNumBlocks(), window->clientModel->getNumBlocksOfPeers());
    window->setEncryptionStatus(window->walletModel->getEncryptionStatus());
    window->walletModel->emitEncryptionStatusChanged(window->walletModel->getEncryptionStatus());
    populateTransactionTable();
    populateAddressTable();
}

// This is just a hook, we won't really be setting the model...
void SpectreBridge::setMessageModel()
{
    populateMessageTable();
    connect(thMessage->mtm, SIGNAL(rowsInserted(QModelIndex,int,int)), SLOT(insertMessages(QModelIndex,int,int)));
    connect(thMessage->mtm, SIGNAL(modelReset()),                      SLOT(populateMessageTable()));
}

void SpectreBridge::copy(QString text)
{
    QApplication::clipboard()->setText(text);
}

void SpectreBridge::paste()
{
    emitPaste(QApplication::clipboard()->text());
}

// Options
void SpectreBridge::populateOptions()
{
    OptionsModel *optionsModel(window->clientModel->getOptionsModel());

    int option = 0;

    QVariantMap options;

    for(option=0;option < optionsModel->rowCount(); option++)
        options.insert(optionsModel->optionIDName(option), optionsModel->index(option).data(Qt::EditRole));

    option = 0;

    QVariantList visibleTransactions;
    QVariantMap notifications;

    while(true)
    {
        QString txType(TransactionRecord::getTypeLabel(option++));

        if(txType.isEmpty())
            break;

        if(visibleTransactions.contains(txType))
        {
            if(txType.isEmpty())
            {
                if(visibleTransactions.length() != 0)
                    break;
            }
            else
                continue;
        }

        visibleTransactions.append(txType);
    }

    QVariantList messageTypes;

    messageTypes.append(tr("Incoming Message"));
    notifications.insert("messages", messageTypes);
    notifications.insert("transactions", visibleTransactions);

    options.insert("optVisibleTransactions", visibleTransactions);
    options.insert("optNotifications",       notifications);

    /* Display elements init */
    QDir translations(":translations");

    QVariantMap languages;

    languages.insert("", "(" + tr("default") + ")");

    foreach(const QString &langStr, translations.entryList())
    {
        QLocale locale(langStr);

        /** display language strings as "native language [- native country] (locale name)", e.g. "Deutsch - Deutschland (de)" */
        languages.insert(langStr, locale.nativeLanguageName() + (langStr.contains("_") ? " - " + locale.nativeCountryName() : "") + " (" + langStr + ")");
    }

    options.insert("optLanguage", languages);

    info->insert("options", options);
}

// Transactions
void SpectreBridge::addRecipient(QString address, QString label, QString narration, qint64 amount, int txnType, int nRingSize)
{
    SendCoinsRecipient rv;


    rv.address = address;
    rv.label = label;
    rv.narration = narration;
    rv.amount = amount;

    std::string sAddr = address.toStdString();
    if (IsBIP32(sAddr.c_str()))
        rv.typeInd = 3;
    else
        rv.typeInd = address.length() > 75 ? AT_Stealth : AT_Normal;

    rv.txnTypeInd = txnType;
    rv.nRingSize = nRingSize;

    recipients.append(rv);

    emit addRecipientResult(true);
}

void SpectreBridge::clearRecipients()
{
    recipients.clear();
}

void SpectreBridge::sendCoins(bool fUseCoinControl, QString sChangeAddr)
{
    int inputTypes = -1;
    int nAnonOutputs = 0;
    int ringSizes = -1;
    // Format confirmation message
    QStringList formatted;
    foreach(const SendCoinsRecipient &rcp, recipients)
    {
        int inputType; // 0 XSPEC, 1 Spectre
        switch(rcp.txnTypeInd)
        {
            case TXT_SPEC_TO_SPEC:
                formatted.append(tr("<b>%1</b> to %2 (%3)").arg(BitcoinUnits::formatWithUnit(BitcoinUnits::XSPEC, rcp.amount), rcp.label.toHtmlEscaped(), rcp.address));
                inputType = 0;
                break;
            case TXT_SPEC_TO_ANON:
                formatted.append(tr("<b>%1</b> to SPECTRE %2 (%3)").arg(BitcoinUnits::formatWithUnit(BitcoinUnits::XSPEC, rcp.amount), rcp.label.toHtmlEscaped(), rcp.address));
                inputType = 0;
                nAnonOutputs++;
                break;
            case TXT_ANON_TO_ANON:
                formatted.append(tr("<b>%1</b> SPECTRE, ring size %2 to SPECTRE %3 (%4)").arg(BitcoinUnits::formatWithUnit(BitcoinUnits::XSPEC, rcp.amount), QString::number(rcp.nRingSize), rcp.label.toHtmlEscaped(), rcp.address));
                inputType = 1;
                nAnonOutputs++;
                break;
            case TXT_ANON_TO_SPEC:
                formatted.append(tr("<b>%1</b> SPECTRE, ring size %2 to XSPEC %3 (%4)").arg(BitcoinUnits::formatWithUnit(BitcoinUnits::XSPEC, rcp.amount), QString::number(rcp.nRingSize), rcp.label.toHtmlEscaped(), rcp.address));
                inputType = 1;
                break;
            default:
                QMessageBox::critical(window, tr("Error:"), tr("Unknown txn type detected %1.").arg(rcp.txnTypeInd),
                              QMessageBox::Abort, QMessageBox::Abort);
                emit sendCoinsResult(false);
                return;
        }

        if (inputTypes == -1)
            inputTypes = inputType;
        else
        if (inputTypes != inputType)
        {
            QMessageBox::critical(window, tr("Error:"), tr("Input types must match for all recipients."),
                          QMessageBox::Abort, QMessageBox::Abort);
            emit sendCoinsResult(false);
            return;
        };

        if (inputTypes == 1)
        {
            if (ringSizes == -1)
                ringSizes = rcp.nRingSize;
            else
            if (ringSizes != rcp.nRingSize)
            {
                QMessageBox::critical(window, tr("Error:"), tr("Ring sizes must match for all recipients."),
                              QMessageBox::Abort, QMessageBox::Abort);
                emit sendCoinsResult(false);
            return;
            };

            if (ringSizes < (int)MIN_RING_SIZE
                || ringSizes > (int)MAX_RING_SIZE)
            {
                QMessageBox::critical(window, tr("Error:"), tr("Ring size outside range [%1, %2].").arg(MIN_RING_SIZE).arg(MAX_RING_SIZE),
                              QMessageBox::Abort, QMessageBox::Abort);
                emit sendCoinsResult(false);
            return;
            };

            if (ringSizes == 1)
            {
                QMessageBox::StandardButton retval = QMessageBox::question(window,
                    tr("Confirm send coins"), tr("Are you sure you want to send?\nRing size of one is not anonymous.").arg(formatted.join(tr(" and "))),
                    QMessageBox::Yes|QMessageBox::Cancel, QMessageBox::Cancel);
                if (retval != QMessageBox::Yes)
                    emit sendCoinsResult(false);
            return;
            };
        };
    };

    QMessageBox::StandardButton retval = QMessageBox::question(window,
        tr("Confirm send coins"), tr("Are you sure you want to send %1?").arg(formatted.join(tr(" and "))),
        QMessageBox::Yes|QMessageBox::Cancel, QMessageBox::Cancel);

    if(retval != QMessageBox::Yes) {
        emit sendCoinsResult(false);
            return;
    }

    WalletModel::SendCoinsReturn sendstatus;

    if (fUseCoinControl)
    {
        if (sChangeAddr.length() > 0)
        {
            CBitcoinAddress addrChange = CBitcoinAddress(sChangeAddr.toStdString());
            if (!addrChange.IsValid())
            {
                QMessageBox::warning(window, tr("Send Coins"),
                    tr("The change address is not valid, please recheck."),
                    QMessageBox::Ok, QMessageBox::Ok);
                emit sendCoinsResult(false);
            return;
            };

            CoinControlDialog::coinControl->destChange = addrChange.Get();
        } else
            CoinControlDialog::coinControl->destChange = CNoDestination();
    };

    WalletModel::UnlockContext ctx(window->walletModel->requestUnlock());

    // Unlock wallet was cancelled
    if(!ctx.isValid()) {
        emit sendCoinsResult(false);
            return;
    }

    if (inputTypes == 1 || nAnonOutputs > 0)
        sendstatus = window->walletModel->sendCoinsAnon(recipients, fUseCoinControl ? CoinControlDialog::coinControl : NULL);
    else
        sendstatus = window->walletModel->sendCoins    (recipients, fUseCoinControl ? CoinControlDialog::coinControl : NULL);

    switch(sendstatus.status)
    {
        case WalletModel::InvalidAddress:
            QMessageBox::warning(window, tr("Send Coins"),
                tr("The recipient address is not valid, please recheck."),
                QMessageBox::Ok, QMessageBox::Ok);
            emit sendCoinsResult(false);
            return;
        case WalletModel::InvalidAmount:
            QMessageBox::warning(window, tr("Send Coins"),
                tr("The amount to pay must be larger than 0."),
                QMessageBox::Ok, QMessageBox::Ok);
            emit sendCoinsResult(false);
            return;
        case WalletModel::AmountExceedsBalance:
            QMessageBox::warning(window, tr("Send Coins"),
                tr("The amount exceeds your balance."),
                QMessageBox::Ok, QMessageBox::Ok);
            emit sendCoinsResult(false);
            return;
        case WalletModel::AmountWithFeeExceedsBalance:
            QMessageBox::warning(window, tr("Send Coins"),
                tr("The total exceeds your balance when the %1 transaction fee is included.").
                arg(BitcoinUnits::formatWithUnit(BitcoinUnits::XSPEC, sendstatus.fee)),
                QMessageBox::Ok, QMessageBox::Ok);
            emit sendCoinsResult(false);
            return;
        case WalletModel::DuplicateAddress:
            QMessageBox::warning(window, tr("Send Coins"),
                tr("Duplicate address found, can only send to each address once per send operation."),
                QMessageBox::Ok, QMessageBox::Ok);
            emit sendCoinsResult(false);
            return;
        case WalletModel::TransactionCreationFailed:
            QMessageBox::warning(window, tr("Send Coins"),
                tr("Error: Transaction creation failed."),
                QMessageBox::Ok, QMessageBox::Ok);
            emit sendCoinsResult(false);
            return;
        case WalletModel::TransactionCommitFailed:
            QMessageBox::warning(window, tr("Send Coins"),
                tr("Error: The transaction was rejected. This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here."),
                QMessageBox::Ok, QMessageBox::Ok);
            emit sendCoinsResult(false);
            return;
        case WalletModel::NarrationTooLong:
            QMessageBox::warning(window, tr("Send Coins"),
                tr("Error: Narration is too long."),
                QMessageBox::Ok, QMessageBox::Ok);
            emit sendCoinsResult(false);
            return;
        case WalletModel::RingSizeError:
            QMessageBox::warning(window, tr("Send Coins"),
                tr("Error: Ring Size Error."),
                QMessageBox::Ok, QMessageBox::Ok);
            emit sendCoinsResult(false);
            return;
        case WalletModel::InputTypeError:
            QMessageBox::warning(window, tr("Send Coins"),
                tr("Error: Input Type Error."),
                QMessageBox::Ok, QMessageBox::Ok);
            emit sendCoinsResult(false);
            return;
        case WalletModel::SCR_NeedFullMode:
            QMessageBox::warning(window, tr("Send Coins"),
                tr("Error: Must be in full mode to send anon."),
                QMessageBox::Ok, QMessageBox::Ok);
            emit sendCoinsResult(false);
            return;
        case WalletModel::SCR_StealthAddressFail:
            QMessageBox::warning(window, tr("Send Coins"),
                tr("Error: Invalid Stealth Address."),
                QMessageBox::Ok, QMessageBox::Ok);
            emit sendCoinsResult(false);
            return;
        case WalletModel::SCR_AmountWithFeeExceedsSpectreBalance:
            QMessageBox::warning(window, tr("Send Coins"),
                tr("The total exceeds your spectre balance when the %1 transaction fee is included.").arg(BitcoinUnits::formatWithUnit(BitcoinUnits::XSPEC, sendstatus.fee)),
                QMessageBox::Ok, QMessageBox::Ok);
            emit sendCoinsResult(false);
            return;
        case WalletModel::SCR_Error:
            QMessageBox::warning(window, tr("Send Coins"),
                tr("Error generating transaction."),
                QMessageBox::Ok, QMessageBox::Ok);
            emit sendCoinsResult(false);
            return;
        case WalletModel::SCR_ErrorWithMsg:
            QMessageBox::warning(window, tr("Send Coins"),
                tr("Error generating transaction: %1").arg(sendstatus.hex),
                QMessageBox::Ok, QMessageBox::Ok);
            emit sendCoinsResult(false);
            return;

        case WalletModel::Aborted: // User aborted, nothing to do
            emit sendCoinsResult(false);
            return;
        case WalletModel::OK:
            //accept();
            CoinControlDialog::coinControl->UnSelectAll();
            CoinControlDialog::payAmounts.clear();
            CoinControlDialog::updateLabels(window->walletModel, 0, this);
            recipients.clear();
            break;
    }

    emit sendCoinsResult(true);
            return;
}

void SpectreBridge::openCoinControl()
{
    if (!window || !window->walletModel)
        return;

    CoinControlDialog dlg;
    dlg.setModel(window->walletModel);
    dlg.exec();

    CoinControlDialog::updateLabels(window->walletModel, 0, this);
}

void SpectreBridge::updateCoinControlAmount(qint64 amount)
{
    CoinControlDialog::payAmounts.clear();
    CoinControlDialog::payAmounts.append(amount);
    CoinControlDialog::updateLabels(window->walletModel, 0, this);
}

void SpectreBridge::updateCoinControlLabels(unsigned int &quantity, int64_t &amount, int64_t &fee, int64_t &afterfee, unsigned int &bytes, QString &priority, QString low, int64_t &change)
{
    emitCoinControlUpdate(quantity, amount, fee, afterfee, bytes, priority, low, change);
}

QVariantMap SpectreBridge::listAnonOutputs()
{
    QVariantMap anonOutputs;
    typedef std::map<int64_t, int> outputCount;

    outputCount mOwnedOutputCounts;
    outputCount mMatureOutputCounts;
    outputCount mSystemOutputCounts;

    if (pwalletMain->CountOwnedAnonOutputs(mOwnedOutputCounts,  false) != 0
     || pwalletMain->CountOwnedAnonOutputs(mMatureOutputCounts, true)  != 0)
    {
        LogPrintf("Error: CountOwnedAnonOutputs failed.\n");
        return anonOutputs;
    };

    for (std::map<int64_t, CAnonOutputCount>::iterator mi(mapAnonOutputStats.begin()); mi != mapAnonOutputStats.end(); mi++)
        mSystemOutputCounts[mi->first] = 0;

    if (pwalletMain->CountAnonOutputs(mSystemOutputCounts, true) != 0)
    {
        LogPrintf("Error: CountAnonOutputs failed.\n");
        return anonOutputs;
    };

    for (std::map<int64_t, CAnonOutputCount>::iterator mi(mapAnonOutputStats.begin()); mi != mapAnonOutputStats.end(); mi++)
    {
        CAnonOutputCount* aoc = &mi->second;
        QVariantMap anonOutput;

        int nDepth = aoc->nLeastDepth == 0 ? 0 : nBestHeight - aoc->nLeastDepth;

        anonOutput.insert("owned_mature",   mMatureOutputCounts[aoc->nValue]);
        anonOutput.insert("owned_outputs",  mOwnedOutputCounts [aoc->nValue]);
        anonOutput.insert("system_mature",  mSystemOutputCounts[aoc->nValue]);
        anonOutput.insert("system_outputs", aoc->nExists);
        anonOutput.insert("system_spends",  aoc->nSpends);
        anonOutput.insert("least_depth",    nDepth);
        anonOutput.insert("value_s",        BitcoinUnits::format(window->clientModel->getOptionsModel()->getDisplayUnit(), aoc->nValue));

        anonOutputs.insert(QString::number(aoc->nValue), anonOutput);
    };

    return anonOutputs;
};

void SpectreBridge::populateTransactionTable()
{
    if(transactionModel->thread() == thread())
    {
        transactionModel->init(window->clientModel, window->walletModel->getTransactionTableModel());
        connect(transactionModel, SIGNAL(emitTransactions(QVariantList)), SIGNAL(emitTransactions(QVariantList)), Qt::QueuedConnection);
        transactionModel->moveToThread(async);
    }

    transactionModel->populatePage();
}

void SpectreBridge::updateTransactions(QModelIndex topLeft, QModelIndex bottomRight)
{
    // Updated transactions...
    if(topLeft.column() == TransactionTableModel::Status)
        transactionModel->populateRows(topLeft.row(), bottomRight.row());
}

void SpectreBridge::insertTransactions(const QModelIndex & parent, int start, int end)
{
    // New Transactions...
    transactionModel->populateRows(start, end);
}

void SpectreBridge::transactionDetails(QString txid)
{
    emit transactionDetailsResult(window->walletModel->getTransactionTableModel()->index(window->walletModel->getTransactionTableModel()->lookupTransaction(txid), 0).data(TransactionTableModel::LongDescriptionRole).toString());
}

// Addresses
void SpectreBridge::populateAddressTable()
{
    if(addressModel->thread() == thread())
    {
        addressModel->atm = window->walletModel->getAddressTableModel();
        connect(addressModel->atm,            SIGNAL(dataChanged(QModelIndex,QModelIndex)), SLOT(updateAddresses(QModelIndex,QModelIndex)));
        connect(addressModel->atm,            SIGNAL(rowsInserted(QModelIndex,int,int)),    SLOT(insertAddresses(QModelIndex,int,int)));
        connect(addressModel, SIGNAL(emitAddresses(QVariantList)), SIGNAL(emitAddresses(QVariantList)), Qt::QueuedConnection);
        addressModel->moveToThread(async);
    }

    addressModel->populateAddressTable();
}

void SpectreBridge::updateAddresses(QModelIndex topLeft, QModelIndex bottomRight)
{
    addressModel->poplateRows(topLeft.row(), bottomRight.row());
}

void SpectreBridge::insertAddresses(const QModelIndex & parent, int start, int end)
{
    // NOTE: Check inInitialBlockDownload here as many stealth addresses uncovered can slow wallet
    //       fPassGuiAddresses allows addresses added manually to still reflect
    if (!fPassGuiAddresses
        && (window->clientModel->inInitialBlockDownload() || addressModel->isRunning()))
        return;

    addressModel->poplateRows(start, end);
}

void SpectreBridge::newAddress(QString addressLabel, int addressType, QString address, bool send)
{
    // Generate a new address to associate with given label
    // NOTE: unlock happens in addRow
    QString rv = addressModel->atm->addRow(send ? AddressTableModel::Send : AddressTableModel::Receive, addressLabel, address, addressType);
    addressModel->populateAddressTable();
    emit newAddressResult(rv);
}

//replica  of the above method for Javascript to diffrentiate when call backs are needed
void SpectreBridge::newAddress_2(QString addressLabel, int addressType, QString address, bool send)
{
    // Generate a new address to associate with given label
    // NOTE: unlock happens in addRow
    QString rv = addressModel->atm->addRow(send ? AddressTableModel::Send : AddressTableModel::Receive, addressLabel, address, addressType);
    addressModel->populateAddressTable();
    emit newAddress_2Result(rv);
}

void SpectreBridge::lastAddressError()
{
    QString sError;
    AddressTableModel::EditStatus status = addressModel->atm->getEditStatus();

    switch(status)
    {
        case AddressTableModel::OK:
        case AddressTableModel::NO_CHANGES: // error?
            break;
        case AddressTableModel::INVALID_ADDRESS:
            sError = "Invalid Address.";
            break;
        case AddressTableModel::DUPLICATE_ADDRESS:
            sError = "Duplicate Address.";
            break;
        case AddressTableModel::WALLET_UNLOCK_FAILURE:
            sError = "Unlock Failed.";
            break;
        case AddressTableModel::KEY_GENERATION_FAILURE:
        default:
            sError = "Unspecified error.";
            break;
    };

    emit lastAddressErrorResult(sError);
}

QString SpectreBridge::getAddressLabel(QString address)
{
    QString result = addressModel->atm->labelForAddress(address);
    emit getAddressLabelResult(result);
    return result;
}

void SpectreBridge::getAddressLabel_2(QString address)
{
    emit getAddressLabel_2Result(addressModel->atm->labelForAddress(address));
}

void SpectreBridge::updateAddressLabel(QString address, QString label)
{
    QString actualLabel = getAddressLabel(address);

    if(actualLabel.startsWith("group_"))
        label = "group_" + label;
    else if(label.startsWith("group_"))
        return;

    addressModel->atm->setData(addressModel->atm->index(addressModel->atm->lookupAddress(address), addressModel->atm->Label), QVariant(label), Qt::EditRole);
}

void SpectreBridge::validateAddress(QString address)
{
    bool result = window->walletModel->validateAddress(address);
    emit validateAddressResult(result);
}

bool SpectreBridge::deleteAddress(QString address)
{
    return addressModel->atm->removeRow(addressModel->atm->lookupAddress(address));
}

// Messages
void SpectreBridge::appendMessages(QString messages, bool reset)
{
    emitMessages("[" + messages + "]", reset);
}

void SpectreBridge::appendMessage(int row)
{
    emitMessage(window->messageModel->index(row, MessageModel::Key).data().toString().toHtmlEscaped(),
                window->messageModel->index(row, MessageModel::Type).data().toString().toHtmlEscaped(),
                window->messageModel->index(row, MessageModel::SentDateTime)    .data().toDateTime().toTime_t(),
                window->messageModel->index(row, MessageModel::ReceivedDateTime).data().toDateTime().toTime_t(),
                window->messageModel->index(row, MessageModel::Label).data(MessageModel::LabelRole).toString().toHtmlEscaped(),
                window->messageModel->index(row, MessageModel::Label).data().toString().replace("\"","\\\"").replace("\\", "\\\\").replace("/", "\\/").toHtmlEscaped(),
                window->messageModel->index(row, MessageModel::LabelTo).data().toString().replace("\"","\\\"").replace("\\", "\\\\").replace("/", "\\/").toHtmlEscaped(),
                window->messageModel->index(row, MessageModel::ToAddress).data().toString().toHtmlEscaped(),
                window->messageModel->index(row, MessageModel::FromAddress).data().toString().toHtmlEscaped(),
                window->messageModel->index(row, MessageModel::Read)            .data().toBool(),
                window->messageModel->index(row, MessageModel::Message).data().toString().toHtmlEscaped());
}

void SpectreBridge::populateMessageTable()
{
    thMessage->mtm = window->messageModel;

    connect(thMessage, SIGNAL(emitMessages(QString, bool)), SLOT(appendMessages(QString, bool)));
    thMessage->start();
}

void SpectreBridge::insertMessages(const QModelIndex & parent, int start, int end)
{
    while(start <= end)
    {
        appendMessage(start++);
        qApp->processEvents();
    }
}

void SpectreBridge::deleteMessage(QString key)
{
    window->messageModel->removeRow(thMessage->mtm->lookupMessage(key));
}

void SpectreBridge::markMessageAsRead(QString key)
{
    window->messageModel->markMessageAsRead(key);
}

QString SpectreBridge::getPubKey(QString address)
{
    return addressModel->atm->pubkeyForAddress(address);;
}

bool SpectreBridge::setPubKey(QString address, QString pubkey)
{
    std::string sendTo = address.toStdString();
    std::string pbkey  = pubkey.toStdString();

    int res = SecureMsgAddAddress(sendTo, pbkey);
    return res == 0||res == 4;
}

void SpectreBridge::sendMessage(const QString &address, const QString &message, const QString &from)
{
    WalletModel::UnlockContext ctx(window->walletModel->requestUnlock());

    // Unlock wallet was cancelled
    if(!ctx.isValid()) {
        emit sendMessageResult(false);
        return;
    }

    MessageModel::StatusCode sendstatus = thMessage->mtm->sendMessage(address, message, from);

    switch(sendstatus)
    {
    case MessageModel::InvalidAddress:
        QMessageBox::warning(window, tr("Send Message"),
            tr("The recipient address is not valid, please recheck."),
            QMessageBox::Ok, QMessageBox::Ok);
        emit sendMessageResult(false);
        return;
    case MessageModel::InvalidMessage:
        QMessageBox::warning(window, tr("Send Message"),
            tr("The message can't be empty."),
            QMessageBox::Ok, QMessageBox::Ok);
        emit sendMessageResult(false);
        return;
    case MessageModel::DuplicateAddress:
        QMessageBox::warning(window, tr("Send Message"),
            tr("Duplicate address found, can only send to each address once per send operation."),
            QMessageBox::Ok, QMessageBox::Ok);
        emit sendMessageResult(false);
        return;
    case MessageModel::MessageCreationFailed:
        QMessageBox::warning(window, tr("Send Message"),
            tr("Error: Message creation failed."),
            QMessageBox::Ok, QMessageBox::Ok);
        emit sendMessageResult(false);
        return;
    case MessageModel::MessageCommitFailed:
        QMessageBox::warning(window, tr("Send Message"),
            tr("Error: The message was rejected."),
            QMessageBox::Ok, QMessageBox::Ok);
        emit sendMessageResult(false);
        return;
    case MessageModel::Aborted:             // User aborted, nothing to do
        emit sendMessageResult(false);
        return;
    case MessageModel::FailedErrorShown:    // Send failed, error message was displayed
        emit sendMessageResult(false);
        return;
    case MessageModel::OK:
        break;
    }

    emit sendMessageResult(true);
    return;
}

void SpectreBridge::createGroupChat(QString label)
{
    //return address to invite to people to.
    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::string strLabel = "group_" + label.toStdString();

    RandAddSeedPerfmon(); // util.cpp
    CKey secret; // hey.h
    secret.MakeNewKey(true);
    CPubKey pubkey = secret.GetPubKey();
    CKeyID vchAddress = pubkey.GetID();

    pwalletMain->MarkDirty();
    CBitcoinAddress addr(vchAddress);

    std::string strAddress = addr.ToString();

    pwalletMain->SetAddressBookName(addr.Get(), strLabel, NULL, true, true);

    if (!pwalletMain->AddKeyPubKey(secret, pubkey)) {
        emit createGroupChatResult("false");
        return;
    }

    SecureMsgAddWalletAddresses();

    emit createGroupChatResult(QString::fromStdString(strAddress));
}


QString SpectreBridge::joinGroupChat(QString privkey, QString label)
{
    /*
    EXPERIMENTAL CODE, UNTESTED.
    */
    std::string strSecret = privkey.toStdString();
    std::string strLabel = "group_" + label.toStdString();

    int64_t nCreateTime = 1;
    CBitcoinSecret vchSecret;
    bool fGood = vchSecret.SetString(strSecret);

    if (!fGood) return "false"; //throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key");
    if (fWalletUnlockStakingOnly) return "false"; //throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Wallet is unlocked for staking only.");

    CKey key = vchSecret.GetKey();
    CPubKey pubkey = key.GetPubKey();
    CKeyID vchAddress = pubkey.GetID();
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);

        pwalletMain->MarkDirty();
        pwalletMain->SetAddressBookName(vchAddress, strLabel);

        // Don't throw error in case a key is already there
        if (pwalletMain->HaveKey(vchAddress))
            return "false";

        pwalletMain->mapKeyMetadata[vchAddress].nCreateTime = nCreateTime;

        if (!pwalletMain->AddKeyPubKey(key, pubkey))
            return "false";
            //throw JSONRPCError(RPC_WALLET_ERROR, "Error adding key to wallet");

        // whenever a key is imported, we need to scan the whole chain
        pwalletMain->nTimeFirstKey = nCreateTime; // 0 would be considered 'no value'

    }

    SecureMsgAddWalletAddresses();
    //TODO: return address and appendAddress with javascript
    CBitcoinAddress addr(vchAddress);
    return QString::fromStdString(addr.ToString());
}


QVariantList SpectreBridge::inviteGroupChat(QString qsaddress, QVariantList invites, QString from)
{
    //TODO: check if part of HD wallet, if it is refuse to send invites.
    QVariantList r; //Return

    QString actualLabel = getAddressLabel(qsaddress);

    if(!actualLabel.startsWith("group_")){
        LogPrintf("[inviteGroupChat] -- This should never happen, if it does please notify devteam.\n");
        QMessageBox::warning(window, tr("Sanity Error!"),
            tr("Error: a sanity check prevented the transfer of a non-group private key, please close your wallet and report this error to the development team as soon as possible."),
            QMessageBox::Ok, QMessageBox::Ok);
    } else {
        actualLabel.replace("group_","");
    }

    QString informText = "Are you sure you want to invite the following addresses to this group?\n";

    for(int i = 0; i < invites.size(); i++)
    {
        QString inviteAddress = invites.at(i).toString();
        QString inviteLabel = getAddressLabel(inviteAddress);
        informText.append(inviteLabel + " -- " + inviteAddress + "\n");
    }

    QMessageBox msgBox;
    msgBox.setStyleSheet("QLabel{min-width: 600px;}");
    msgBox.setText("Inviting to group " + actualLabel);
    msgBox.setInformativeText(informText);
    msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Ok);

    if(msgBox.exec() == QMessageBox::Cancel)
    {
        LogPrintf("[inviteGroupChat] -- inviteGroupChat aborted.\n");
        r.append("error");
        return r;
    }

    LogPrintf("[inviteGroupChat] -- start\n");
    CBitcoinAddress address;

    if (!address.SetString(qsaddress.toStdString()))
    {
        LogPrintf("[inviteGroupChat] -- SetString address failed.\n");
        r.append("error");
        return r;
    }

    CKeyID keyID;
    if (!address.GetKeyID(keyID))
    {
        LogPrintf("[inviteGroupChat] -- GetKeyID failed.\n");
        r.append("error");
        return r;
    }

    CKey vchSecret;

    if (!pwalletMain->GetKey(keyID, vchSecret))
    {
        LogPrintf("[inviteGroupChat] -- GetKey failed.\n");
        r.append("error");
        return r;
    }

    QString message = "/invite " + QString::fromStdString(CBitcoinSecret(vchSecret).ToString()) + " " + actualLabel;

    //SecureString privkey(); //.reserve then .assign(CBitcoinSecret(vchSecret).ToString()))

    for(int i = 0; i < invites.size(); i++)
    {
        QString inviteAddress = invites.at(i).toString();
        LogPrintf("[inviteGroupChat] sending invite!");

        if(thMessage->mtm->sendMessage(inviteAddress, message, from) == MessageModel::OK)
            r.append(inviteAddress);
    }

    return invites;
}

QString SpectreBridge::translateHtmlString(QString string)
{
    int i = 0;
    while (html_strings[i] != 0)
    {
        if (html_strings[i] == string)
            return tr(html_strings[i]);

        i++;
    }

    return string;
}

QJsonValue SpectreBridge::userAction(QJsonValue action)
{
    QJsonArray array;

    QString key = action.toArray().at(0).toString();

    if(key == "backupWallet")
        window->backupWallet();
    if(key == "close")
        window->close();
    if(key == "encryptWallet")
        window->encryptWallet(true);
    if(key == "changePassphrase")
        window->changePassphrase();
    if(key == "toggleLock")
        window->toggleLock();
    if(key == "developerConsole")
        window->webEngineView->page()->triggerAction(QWebEnginePage::InspectElement);
    if(key == "aboutClicked")
        window->aboutClicked();
    if(key == "aboutQtClicked")
        window->aboutQtAction->trigger();
    if(key == "debugClicked")
        window->rpcConsole->show();
    if(key == "clearRecipients")
        clearRecipients();
    //TODO: Port this part of the code to the new json based system
//    if(key == "optionsChanged")
//    {
//        OptionsModel * optionsModel(window->clientModel->getOptionsModel());
//        QVariantMap value(it.value().toMap());

//        for(int option = 0;option < optionsModel->rowCount(); option++)
//            if(value.contains(optionsModel->optionIDName(option)))
//                optionsModel->setData(optionsModel->index(option), value.value(optionsModel->optionIDName(option)));

//        populateOptions();
//    }

    return QJsonValue();
}

// Blocks
void SpectreBridge::listLatestBlocks()
{
    CBlockIndex* recentBlock = pindexBest;
    CBlock block;
    QVariantMap latestBlocks;


    for (int x = 0; x < 5 && recentBlock; x++)
    {

        block.ReadFromDisk(recentBlock, true);

        if (block.IsNull() || block.vtx.size() < 1)
        {
            latestBlocks.insert("error_msg", "Block not found.");
            emit listLatestBlocksResult(latestBlocks);
            return;
        };

        QVariantMap latestBlock;
        latestBlock.insert("block_hash"        , QString::fromStdString(recentBlock->GetBlockHash().ToString()));
        latestBlock.insert("block_height"      , recentBlock->nHeight);
        latestBlock.insert("block_timestamp"   , DateTimeStrFormat("%x %H:%M:%S", recentBlock->GetBlockTime()).c_str());
        latestBlock.insert("block_transactions", QString::number(block.vtx.size() - 1));
        latestBlock.insert("block_size"        , recentBlock->nBits);
        latestBlocks.insert(QString::number(x) , latestBlock);
        recentBlock = recentBlock->pprev;
    }
    emit listLatestBlocksResult(latestBlocks);
    return;
}

void SpectreBridge::findBlock(QString searchID)
{
    CBlockIndex* findBlock;

    int blkHeight = searchID.toInt();

    QVariantMap foundBlock;

    if (blkHeight != 0 || searchID == "0")
    {
        findBlock = FindBlockByHeight(blkHeight);
    } else
    {
        uint256 hash, hashBlock;
        hash.SetHex(searchID.toStdString());

        // -- look for a block or transaction
        //    Note: only finds transactions in the block chain
        std::map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hash);
        if (mi != mapBlockIndex.end()
            || (GetTransactionBlockHash(hash, hashBlock)
                && (mi = mapBlockIndex.find(hashBlock)) != mapBlockIndex.end()))
        {
            findBlock = mi->second;
        } else
        {
            findBlock = NULL;
        };
    };

    if (!findBlock)
    {
        foundBlock.insert("error_msg", "Block / transaction not found.");
        emit findBlockResult(foundBlock);
        return;
    };

    CBlock block;
    block.ReadFromDisk(findBlock, true);

    if (block.IsNull() || block.vtx.size() < 1)
    {
        foundBlock.insert("error_msg", "Block not found.");
        emit findBlockResult(foundBlock);
        return;
    };

    foundBlock.insert("block_hash"        , QString::fromStdString(findBlock->GetBlockHash().ToString()));
    foundBlock.insert("block_height"      , findBlock->nHeight);
    foundBlock.insert("block_timestamp"   , DateTimeStrFormat("%x %H:%M:%S", findBlock->GetBlockTime()).c_str());
    foundBlock.insert("block_transactions", QString::number(block.vtx.size() - 1));
    foundBlock.insert("block_size"        , findBlock->nBits);
    foundBlock.insert("error_msg"         , "");

    emit findBlockResult(foundBlock);
}

void SpectreBridge::blockDetails(QString blkHash)
{
    QVariantMap blockDetail;

    uint256 hash;
    hash.SetHex(blkHash.toStdString());

    CBlockIndex* blkIndex;
    CBlock block;

    std::map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hash);
    if (mi == mapBlockIndex.end())
    {
        blockDetail.insert("error_msg", "Block not found.");
        emit blockDetailsResult(blockDetail);
        return;
    };

    blkIndex  = mi->second;
    block.ReadFromDisk(blkIndex, true);

    if (block.IsNull() || block.vtx.size() < 1)
    {
        blockDetail.insert("error_msg", "Block not found.");
        emit blockDetailsResult(blockDetail);
        return;
    };

    CTxDB txdb("r");
    MapPrevTx mapInputs;
    std::map<uint256, CTxIndex> mapUnused;
    bool fInvalid = false;
    int64_t nTxValueIn = 0, nTxValueOut = 0;
    std::string sBlockType = "";
    if (block.IsProofOfStake())
    {
        sBlockType = "PoS";

        CTransaction& coinstake = block.vtx[1]; // IsProofOfStake checks vtx > 1

        if (coinstake.FetchInputs(txdb, mapUnused, false, false, mapInputs, fInvalid))
            nTxValueIn = coinstake.GetValueIn(mapInputs);
        // else error

        nTxValueOut = coinstake.GetValueOut();
    } else
    {
        sBlockType = "PoW";

        CTransaction& coinbase = block.vtx[0]; // check vtx.size() above

        if (coinbase.FetchInputs(txdb, mapUnused, false, false, mapInputs, fInvalid))
            nTxValueIn = coinbase.GetValueIn(mapInputs);
        // else error

        nTxValueOut = coinbase.GetValueOut();
    };

    double nBlockReward = (double)(nTxValueOut - nTxValueIn) / (double)COIN;


    std::string sHashPrev = blkIndex->pprev ? blkIndex->pprev->GetBlockHash().ToString() : "None";
    std::string sHashNext = blkIndex->pnext ? blkIndex->pnext->GetBlockHash().ToString() : "None";

    blockDetail.insert("block_hash"        , QString::fromStdString(blkIndex->GetBlockHash().ToString()));
    blockDetail.insert("block_transactions", QString::number(block.vtx.size() - 1));
    blockDetail.insert("block_height"      , blkIndex->nHeight);
    blockDetail.insert("block_type"        , QString::fromStdString(sBlockType));
    blockDetail.insert("block_reward"      , QString::number(nBlockReward));
    blockDetail.insert("block_timestamp"   , DateTimeStrFormat("%x %H:%M:%S", blkIndex->GetBlockTime()).c_str());
    blockDetail.insert("block_merkle_root" , QString::fromStdString(blkIndex->hashMerkleRoot.ToString()));
    blockDetail.insert("block_prev_block"  , QString::fromStdString(sHashPrev));
    blockDetail.insert("block_next_block"  , QString::fromStdString(sHashNext));
    blockDetail.insert("block_difficulty"  , GetDifficulty(blkIndex));
    blockDetail.insert("block_bits"        , blkIndex->nBits);
    blockDetail.insert("block_size"        , (int)::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION));
    blockDetail.insert("block_version"     , blkIndex->nVersion);
    blockDetail.insert("block_nonce"       , blkIndex->nNonce);
    blockDetail.insert("error_msg"         , "");

    emit blockDetailsResult(blockDetail);
    return;
}

void SpectreBridge::listTransactionsForBlock(QString blkHash)
{
    QVariantMap blkTransactions;

    uint256 hash;
    hash.SetHex(blkHash.toStdString());

    CBlockIndex* selectedBlkIndex;
    CBlock block;

    std::map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hash);
    if (mi == mapBlockIndex.end())
    {
        blkTransactions.insert("error_msg", "Block not found.");
        emit listTransactionsForBlockResult(blkTransactions);
        return;
    };

    selectedBlkIndex  = mi->second;
    block.ReadFromDisk(selectedBlkIndex, true);

    if (block.IsNull() || block.vtx.size() < 1)
    {
        blkTransactions.insert("error_msg", "Block not found.");
        emit listTransactionsForBlockResult(blkTransactions);
        return;
    };

    for (uint x = 0; x < block.vtx.size(); x++)
    {
        if(block.vtx[x].GetValueOut() == 0)
            continue;

        QVariantMap blockTxn;
        CTransaction txn;
        txn = block.vtx[x];

        blockTxn.insert("transaction_hash"       , QString::fromStdString(txn.GetHash().ToString()));
        blockTxn.insert("transaction_value"      , QString::number(txn.GetValueOut() / (double)COIN));
        blkTransactions.insert(QString::number(x), blockTxn);
    }

    emit listTransactionsForBlockResult(blkTransactions);
    return;
}

void SpectreBridge::txnDetails(QString blkHash, QString txnHash)
{
    QVariantMap txnDetail;

    uint256 hash;
    hash.SetHex(blkHash.toStdString());

    uint256 thash;
    thash.SetHex(txnHash.toStdString());

    CBlockIndex* selectedBlkIndex;
    CBlock block;

    std::map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hash);
    if (mi == mapBlockIndex.end())
    {
        txnDetail.insert("error_msg", "Block not found.");
        emit txnDetailsResult(txnDetail);
        return;
    };
    selectedBlkIndex  = mi->second;
    block.ReadFromDisk(selectedBlkIndex, true);

    if (block.IsNull() || block.vtx.size() < 1)
    {
        txnDetail.insert("error_msg", "Block not found.");
        emit txnDetailsResult(txnDetail);
        return;
    };

    for (uint x = 0; x < block.vtx.size(); x++)
    {

        if(block.vtx[x].GetHash() != thash)
            continue;

        CTransaction txn;
        CTxIndex txIdx;
        CTxDB txdb("r");
        MapPrevTx mapInputs;
        std::map<uint256, CTxIndex> mapUnused;
        txdb.ReadDiskTx(thash, txn, txIdx);

        bool fInvalid = false;
        int64_t nTxValueIn = 0, nTxValueOut = 0;

        if (txn.FetchInputs(txdb, mapUnused, false, false, mapInputs, fInvalid))
            nTxValueIn = txn.GetValueIn(mapInputs);
        else continue;

        nTxValueOut = txn.GetValueOut();
        double nTxnRewardOrFee = (double)(nTxValueOut - nTxValueIn) / (double)COIN;

        // Lets start collecting the INPUTS
        QVariantMap inputDetails;

        for (uint32_t i = 0; i < txn.vin.size(); ++i)
        {
            QVariantMap inputDetail;
            const CTxIn& txin = txn.vin[i];

            std::string sAddr = "";
            std::string sCoinValue;

            if (txn.nVersion == ANON_TXN_VERSION
                && txin.IsAnonInput())
            {
                sAddr = "Spectre";
                std::vector<uint8_t> vchImage;
                txin.ExtractKeyImage(vchImage);

                CKeyImageSpent ski;
                bool fInMemPool;
                if (GetKeyImage(&txdb, vchImage, ski, fInMemPool))
                {
                    sCoinValue = strprintf("%f", (double)ski.nValue / (double)COIN);
                } else
                {
                    sCoinValue = "spend not in chain!";
                };

            } else
            {
                CTransaction prevTx;
                if (txdb.ReadDiskTx(txin.prevout.hash, prevTx))
                {
                    if (txin.prevout.n < prevTx.vout.size())
                    {
                        const CTxOut &vout = prevTx.vout[txin.prevout.n];
                        sCoinValue = strprintf("%f", (double)vout.nValue / (double)COIN);

                        CTxDestination address;
                        if (ExtractDestination(vout.scriptPubKey, address))
                            sAddr = CBitcoinAddress(address).ToString();
                    } else
                    {
                        sCoinValue = "out of range";
                    };
                };
            };

            inputDetail.insert("input_source_address", QString::fromStdString(sAddr));
            inputDetail.insert("input_value"         , QString::fromStdString(sCoinValue));
            inputDetails.insert(QString::number(i)   , inputDetail);

        };

        // Lets start collecting the OUTPUTS
        QVariantMap outputDetails;

        for (unsigned int i = 0; i < txn.vout.size(); i++)
        {
            QVariantMap outputDetail;
            const CTxOut& txout = txn.vout[i];

             if (txout.nValue < 1) // metadata output, narration or stealth
                 continue;

             std::string sAddr = "";


             if( txn.nVersion == ANON_TXN_VERSION
                 && txout.IsAnonOutput() )
                 sAddr = "Spectre";
             else
             {
                 CTxDestination address;
                 if (ExtractDestination(txout.scriptPubKey, address))
                    sAddr = CBitcoinAddress(address).ToString();
             }

             outputDetail.insert("output_source_address", QString::fromStdString(sAddr));
             outputDetail.insert("output_value"         , QString::number((double)txout.nValue / (double)COIN ));
             outputDetails.insert(QString::number(i)    , outputDetail);
        };

        txnDetail.insert("transaction_hash"         , QString::fromStdString(txn.GetHash().ToString()));
        txnDetail.insert("transaction_value"        , QString::number(txn.GetValueOut() / (double)COIN));
        txnDetail.insert("transaction_size"         , (int)::GetSerializeSize(txn, SER_NETWORK, PROTOCOL_VERSION));
        txnDetail.insert("transaction_rcv_time"     , DateTimeStrFormat("%x %H:%M:%S", txn.nTime ).c_str());
        txnDetail.insert("transaction_mined_time"   , DateTimeStrFormat("%x %H:%M:%S", block.GetBlockTime()).c_str());
        txnDetail.insert("transaction_block_hash"   , QString::fromStdString(block.GetHash().ToString()));
        txnDetail.insert("transaction_reward"       , QString::number(nTxnRewardOrFee));
        txnDetail.insert("transaction_confirmations", QString::number( txIdx.GetDepthInMainChainFromIndex() ));
        txnDetail.insert("transaction_inputs"       , inputDetails);
        txnDetail.insert("transaction_outputs"      , outputDetails);
        txnDetail.insert("error_msg"                , "");

        break;
    }

    emit txnDetailsResult(txnDetail);
    return;
}

QVariantMap SpectreBridge::signMessage(QString address, QString message)
{
    QVariantMap result;

    CBitcoinAddress addr(address.toStdString());
    if (!addr.IsValid())
    {
        result.insert("error_msg", "The entered address is invalid. Please check the address and try again.");
        return result;
    }
    CKeyID keyID;
    if (!addr.GetKeyID(keyID))
    {
        result.insert("error_msg", "The entered address does not refer to a key. Please check the address and try again.");
        return result;
    }

    WalletModel::UnlockContext ctx(window->walletModel->requestUnlock());
    if (!ctx.isValid())
    {
        result.insert("error_msg", "Wallet unlock was cancelled.");
        return result;
    }

    CKey key;
    if (!pwalletMain->GetKey(keyID, key))
    {
        result.insert("error_msg", "Private key for the entered address is not available.");
        return result;
    }

    CDataStream ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << message.toStdString();

    std::vector<unsigned char> vchSig;
    if (!key.SignCompact(Hash(ss.begin(), ss.end()), vchSig))
    {
        result.insert("error_msg" , "Message signing failed.");
        return result;
    }
    result.insert("signed_signature", QString::fromStdString(EncodeBase64(&vchSig[0], vchSig.size())));
    result.insert("error_msg", "");

    return result;
}

QVariantMap SpectreBridge::verifyMessage(QString address, QString message, QString signature)
{
    QVariantMap result;

    CBitcoinAddress addr(address.toStdString());
    if (!addr.IsValid())
    {
        result.insert("error_msg", "The entered address is invalid. Please check the address and try again.");
        return result;
    }
    CKeyID keyID;
    if (!addr.GetKeyID(keyID))
    {
        result.insert("error_msg", "The entered address does not refer to a key. Please check the address and try again.");
        return result;
    }

    bool fInvalid = false;
    std::vector<unsigned char> vchSig = DecodeBase64(signature.toStdString().c_str(), &fInvalid);

    if (fInvalid)
    {
        result.insert("error_msg", "The signature could not be decoded. Please check the signature and try again.");
        return result;
    }

    CDataStream ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << message.toStdString();

    CPubKey pubkey;
    if (!pubkey.RecoverCompact(Hash(ss.begin(), ss.end()), vchSig))
    {
        result.insert("error_msg", "The signature did not match the message digest. Please check the signature and try again.");
        return result;
    }

    if (!(CBitcoinAddress(pubkey.GetID()) == addr))
    {
        result.insert("error_msg", "Message verification failed.");
        return result;
    }

    // If we get here all went well and the message is valid
    result.insert("error_msg", "");

    return result;
}

void SpectreBridge::getNewMnemonic(QString password, QString language)
{
    QVariantMap result;
    int nLanguage = language.toInt();

    int nBytesEntropy = 32;
    std::string sPassword = password.toStdString();
    std::string sError;
    std::string sKey;
    std::vector<uint8_t> vEntropy;
    std::vector<uint8_t> vSeed;
    bool fBip44 = false;
    vEntropy.resize(nBytesEntropy);
    std::string sMnemonic;
    CExtKey ekMaster;

    RandAddSeedPerfmon();
    for (uint32_t i = 0; i < MAX_DERIVE_TRIES; ++i)
    {
        if (1 != RAND_bytes(&vEntropy[0], nBytesEntropy))
            throw std::runtime_error("RAND_bytes failed.");

        if (0 != MnemonicEncode(nLanguage, vEntropy, sMnemonic, sError))
        {
            result.insert("error_msg", strprintf("MnemonicEncode failed %s.", sError.c_str()).c_str());
            emit getNewMnemonicResult(result);
            return;
        }

        if (0 != MnemonicToSeed(sMnemonic, sPassword, vSeed))
        {
          result.insert("error_msg", "MnemonicToSeed failed.");
          emit getNewMnemonicResult(result);
          return;
        }

        ekMaster.SetMaster(&vSeed[0], vSeed.size());

        CExtKey58 eKey58;

        if (fBip44)
        {
            eKey58.SetKey(ekMaster, CChainParams::EXT_SECRET_KEY_BTC);
        } else
        {
            eKey58.SetKey(ekMaster, CChainParams::EXT_SECRET_KEY);
        };

        sKey = eKey58.ToString();

        if (!ekMaster.IsValid())
              continue;
        break;
    };

    result.insert("error_msg", "");
    result.insert("mnemonic", QString::fromStdString(sMnemonic));
    //result.insert("master", QString::fromStdString(sKey));

    emit getNewMnemonicResult(result);
    return;
}

void SpectreBridge::importFromMnemonic(QString inMnemonic, QString inPassword, QString inLabel, bool fBip44, int64_t nCreateTime)
{
    std::string sPassword = inPassword.toStdString();
    std::string sMnemonic = inMnemonic.toStdString();
    std::string sError;
    std::vector<uint8_t> vEntropy;
    std::vector<uint8_t> vSeed;
    QVariantMap result;

    // - decode to determine validity of mnemonic
    if (0 != MnemonicDecode(-1, sMnemonic, vEntropy, sError))
    {
        result.insert("error_msg", QString::fromStdString(strprintf("MnemonicDecode failed %s.", sError.c_str()).c_str() ));
        emit importFromMnemonicResult(result);
        return;
    }

    if (0 != MnemonicToSeed(sMnemonic, sPassword, vSeed))
    {
        result.insert("error_msg", "MnemonicToSeed failed.");
        emit importFromMnemonicResult(result);
        return;
    }

    CExtKey ekMaster;
    CExtKey58 eKey58;
    ekMaster.SetMaster(&vSeed[0], vSeed.size());

    if (!ekMaster.IsValid())
    {
        result.insert("error_msg", "Invalid key.");
        emit importFromMnemonicResult(result);
        return;
    }

    eKey58.SetKey(ekMaster, CChainParams::EXT_SECRET_KEY);
    /*
    if (fBip44)
    {

        eKey58.SetKey(ekMaster, CChainParams::EXT_SECRET_KEY_BTC);

        //result.push_back(Pair("master", eKey58.ToString()));

        // m / purpose' / coin_type' / account' / change / address_index
        CExtKey ekDerived;
        ekMaster.Derive(ekDerived, BIP44_PURPOSE);
        ekDerived.Derive(ekDerived, Params().BIP44ID());

        eKey58.SetKey(ekDerived, CChainParams::EXT_SECRET_KEY);

        //result.push_back(Pair("derived", eKey58.ToString()));
    } else
    {
        eKey58.SetKey(ekMaster, CChainParams::EXT_SECRET_KEY);
        //result.push_back(Pair("master", eKey58.ToString()));
    };*/

    // - in c++11 strings are definitely contiguous, and before they're very unlikely not to be
    //    OPENSSL_cleanse(&sMnemonic[0], sMnemonic.size());
    //    OPENSSL_cleanse(&sPassword[0], sPassword.size());
    connect(this, SIGNAL(extKeyImportResult(QVariantMap)), this, SIGNAL(importFromMnemonicResult(QVariantMap)));
    extKeyImport(QString::fromStdString(eKey58.ToString()), inLabel, fBip44, nCreateTime);
    return;
}

inline uint32_t reversePlace(uint8_t *p)
{
    uint32_t rv = 0;
    for (int i = 0; i < 4; ++i)
        rv |= (uint32_t) *(p+i) << (8 * (3-i));
    return rv;
};

int KeyInfo(CKeyID &idMaster, CKeyID &idKey, CStoredExtKey &sek, int nShowKeys, QVariantMap &obj, std::string &sError)
{
    CExtKey58 eKey58;

    bool fBip44Root = false;

    obj.insert("type", "Loose");
    obj.insert("active", sek.nFlags & EAF_ACTIVE ? "true" : "false");
    obj.insert("receive_on", sek.nFlags & EAF_RECEIVE_ON ? "true" : "false");
    obj.insert("encrypted", sek.nFlags & EAF_IS_CRYPTED ? "true" : "false");
    obj.insert("label", QString::fromStdString(sek.sLabel));

    if (reversePlace(&sek.kp.vchFingerprint[0]) == 0)
    {
        obj.insert("path", "Root");
    } else
    {
        mapEKValue_t::iterator mi = sek.mapValue.find(EKVT_PATH);
        if (mi != sek.mapValue.end())
        {
            std::string sPath;
            if (0 == PathToString(mi->second, sPath, 'h'))
                obj.insert("path", QString::fromStdString(sPath));
        };
    };

    mapEKValue_t::iterator mi = sek.mapValue.find(EKVT_KEY_TYPE);
    if (mi != sek.mapValue.end())
    {
        uint8_t type = EKT_MAX_TYPES;
        if (mi->second.size() == 1)
            type = mi->second[0];

        std::string sType;
        switch (type)
        {
            case EKT_MASTER      : sType = "Master"; break;
            case EKT_BIP44_MASTER:
                sType = "BIP44 Key";
                fBip44Root = true;
                break;
            default              : sType = "Unknown"; break;
        };
        obj.insert("key_type", QString::fromStdString(sType));
    };

    if (idMaster == idKey)
        obj.insert("current_master", "true");

    CBitcoinAddress addr;
    mi = sek.mapValue.find(EKVT_ROOT_ID);
    if (mi != sek.mapValue.end())
    {
        CKeyID idRoot;

        if (GetCKeyID(mi->second, idRoot))
        {
            addr.Set(idRoot, CChainParams::EXT_KEY_HASH);
            obj.insert("root_key_id", QString::fromStdString(addr.ToString()));
        } else
        {
            obj.insert("root_key_id", "malformed");
        };
    };

    mi = sek.mapValue.find(EKVT_CREATED_AT);
    if (mi != sek.mapValue.end())
    {
        int64_t nCreatedAt;
        GetCompressedInt64(mi->second, (uint64_t&)nCreatedAt);
        obj.insert("created_at", QString::number(nCreatedAt));
    };

    addr.Set(idKey, CChainParams::EXT_KEY_HASH);
    obj.insert("id", QString::fromStdString(addr.ToString()));

    if (nShowKeys > 1
        && pwalletMain->ExtKeyUnlock(&sek) == 0)
    {
        if (fBip44Root)
            eKey58.SetKey(sek.kp, CChainParams::EXT_SECRET_KEY_BTC);
        else
            eKey58.SetKeyV(sek.kp);
        obj.insert("evkey", QString::fromStdString(eKey58.ToString()));
    };

    if (nShowKeys > 0)
    {
        if (fBip44Root)
            eKey58.SetKey(sek.kp, CChainParams::EXT_PUBLIC_KEY_BTC);
        else
            eKey58.SetKeyP(sek.kp);

        obj.insert("epkey", QString::fromStdString(eKey58.ToString()));
    };

    obj.insert("num_derives"         , QString::fromStdString(strprintf("%u", sek.nGenerated)));
    obj.insert("num_derives_hardened", QString::fromStdString(strprintf("%u", sek.nHGenerated)));

    return 0;
}

int AccountInfo(CExtKeyAccount *pa, int nShowKeys, QVariantMap &obj, std::string &sError)
{
    CExtKey58 eKey58;

    obj.insert("type", "Account");
    obj.insert("active", pa->nFlags & EAF_ACTIVE ? "true" : "false");
    obj.insert("label", QString::fromStdString(pa->sLabel));

    if (pwalletMain->idDefaultAccount == pa->GetID())
        obj.insert("default_account", "true");

    mapEKValue_t::iterator mi = pa->mapValue.find(EKVT_CREATED_AT);
    if (mi != pa->mapValue.end())
    {
        int64_t nCreatedAt;
        GetCompressedInt64(mi->second, (uint64_t&)nCreatedAt);

        obj.insert("created_at", QString::fromStdString(DateTimeStrFormat("%x %H:%M:%S", nCreatedAt).c_str()));
    };

    obj.insert("id", QString::fromStdString(pa->GetIDString58()));
    obj.insert("has_secret", pa->nFlags & EAF_HAVE_SECRET ? "true" : "false");

    CStoredExtKey *sekAccount = pa->ChainAccount();
    if (!sekAccount)
    {
        obj.insert("error", "chain account not set.");
        return 0;
    };

    CBitcoinAddress addr;
    addr.Set(pa->idMaster, CChainParams::EXT_KEY_HASH);
    obj.insert("root_key_id", QString::fromStdString(addr.ToString()));

    mi = sekAccount->mapValue.find(EKVT_PATH);
    if (mi != sekAccount->mapValue.end())
    {
        std::string sPath;
        if (0 == PathToString(mi->second, sPath, 'h'))
            obj.insert("path", QString::fromStdString(sPath));
    };
    // TODO: separate passwords for accounts
    if (pa->nFlags & EAF_HAVE_SECRET
        && nShowKeys > 1
        && pwalletMain->ExtKeyUnlock(sekAccount) == 0)
    {
        eKey58.SetKeyV(sekAccount->kp);
        obj.insert("evkey", QString::fromStdString(eKey58.ToString()));
    };

    if (nShowKeys > 0)
    {
        eKey58.SetKeyP(sekAccount->kp);
        obj.insert("epkey", QString::fromStdString(eKey58.ToString()));
    };

    if (pa->nActiveExternal < pa->vExtKeys.size())
    {
        CStoredExtKey *sekE = pa->vExtKeys[pa->nActiveExternal];
        if (nShowKeys > 0)
        {
            eKey58.SetKeyP(sekE->kp);
            obj.insert("external_chain", QString::fromStdString(eKey58.ToString()));
        };
        obj.insert("num_derives_external", QString::fromStdString(strprintf("%u", sekE->nGenerated)));
        obj.insert("num_derives_external_h", QString::fromStdString(strprintf("%u", sekE->nHGenerated)));
    };

    if (pa->nActiveInternal < pa->vExtKeys.size())
    {
        CStoredExtKey *sekI = pa->vExtKeys[pa->nActiveInternal];
        if (nShowKeys > 0)
        {
            eKey58.SetKeyP(sekI->kp);
            obj.insert("internal_chain", QString::fromStdString(eKey58.ToString()));
        };
        obj.insert("num_derives_internal", QString::fromStdString(strprintf("%u", sekI->nGenerated)));
        obj.insert("num_derives_internal_h", QString::fromStdString(strprintf("%u", sekI->nHGenerated)));
    };

    if (pa->nActiveStealth < pa->vExtKeys.size())
    {
        CStoredExtKey *sekS = pa->vExtKeys[pa->nActiveStealth];
        obj.insert("num_derives_stealth", QString::fromStdString(strprintf("%u", sekS->nGenerated)));
        obj.insert("num_derives_stealth_h", QString::fromStdString(strprintf("%u", sekS->nHGenerated)));
    };

    return 0;
};

class GUIListExtCallback : public LoopExtKeyCallback
{
public:
    GUIListExtCallback(QVariantMap *resMap, int _nShowKeys)
    {
        nItems = 0;
        resultMap = resMap;
        nShowKeys = _nShowKeys;

        if (pwalletMain && pwalletMain->pEkMaster)
            idMaster = pwalletMain->pEkMaster->GetID();
    };

    int ProcessKey(CKeyID &id, CStoredExtKey &sek)
    {
        nItems++;
        QVariantMap obj;
        KeyInfo(idMaster, id, sek, nShowKeys, obj, sError);
        resultMap->insert(QString::number(nItems), obj);
        return 0;
    };

    int ProcessAccount(CKeyID &id, CExtKeyAccount &sea)
    {
        nItems++;
        QVariantMap obj;

        AccountInfo(&sea, nShowKeys, obj, sError);
        resultMap->insert(QString::number(nItems), obj);
        return 0;
    };

    std::string sError;
    int nItems;
    int nShowKeys;
    CKeyID idMaster;
    QVariantMap *resultMap;
};

void SpectreBridge::extKeyAccList() {
    QVariantMap result;

    GUIListExtCallback extKeys(&result, 10 );

    {
        LOCK(pwalletMain->cs_wallet);
        LoopExtAccountsInDB(true, extKeys);
    } //cs_wallet

    CBitcoinAddress addr;

    addr.GetKeyID(extKeys.idMaster);

    emit extKeyAccListResult(result);
    return;
}

void SpectreBridge::extKeyList() {
    QVariantMap result;

    GUIListExtCallback extKeys(&result, 10 );

    {
        LOCK(pwalletMain->cs_wallet);
        LoopExtKeysInDB(true, false, extKeys);
    } //cs_wallet

    emit extKeyListResult(result);
    return;
}

void SpectreBridge::extKeyImport(QString inKey, QString inLabel, bool fBip44, int64_t nCreateTime)
{
    QVariantMap result;
    std::string sInKey = inKey.toStdString();
    CStoredExtKey sek;
    sek.sLabel = inLabel.toStdString();

    std::vector<uint8_t> v;
    sek.mapValue[EKVT_CREATED_AT] = SetCompressedInt64(v, nCreateTime ? nCreateTime : GetTime());

    CExtKey58 eKey58;
    if (eKey58.Set58(sInKey.c_str()) == 0)
    {
        if (!eKey58.IsValid(CChainParams::EXT_SECRET_KEY)
         && !eKey58.IsValid(CChainParams::EXT_PUBLIC_KEY_BTC))
        {
            result.insert("error_msg", "Import failed - Key must begin with Spectrecoin prefix.");
            emit extKeyImportResult(result);
            return;
        }

        sek.kp = eKey58.GetKey();
    } else
    {
        result.insert("error_msg", "Import failed - Invalid key.");
        emit extKeyImportResult(result);
        disconnect(this, SIGNAL(extKeyImportResult(QVariantMap)), this, SIGNAL(importFromMnemonicResult(QVariantMap)));
        return;
    };

    {
        LOCK(pwalletMain->cs_wallet);
        CWalletDB wdb(pwalletMain->strWalletFile, "r+");
        if (!wdb.TxnBegin())
        {
            result.insert("error_msg", "TxnBegin failed.");
            emit extKeyImportResult(result);
            disconnect(this, SIGNAL(extKeyImportResult(QVariantMap)), this, SIGNAL(importFromMnemonicResult(QVariantMap)));
            return;
        }
        int rv;
        if (0 != (rv = pwalletMain->ExtKeyImportLoose(&wdb, sek, fBip44, false)))
        {
            wdb.TxnAbort();
            result.insert("error_msg", QString("ExtKeyImportLoose failed, %1").arg(ExtKeyGetString(rv)));
            emit extKeyImportResult(result);
            disconnect(this, SIGNAL(extKeyImportResult(QVariantMap)), this, SIGNAL(importFromMnemonicResult(QVariantMap)));
            return;
        } else
            if (!wdb.TxnCommit())
            {
                result.insert("error_msg", "TxnCommit failed.");
                emit extKeyImportResult(result);
                disconnect(this, SIGNAL(extKeyImportResult(QVariantMap)), this, SIGNAL(importFromMnemonicResult(QVariantMap)));
                return;
            };
    } // cs_wallet

    // set new key as master
    if(fBip44)
    {
        CExtKey ekDerived;
        sek.kp.Derive(ekDerived, BIP44_PURPOSE);
        ekDerived.Derive(ekDerived, Params().BIP44ID());
        sek.kp = ekDerived;
    }
    extKeySetMaster(QString::fromStdString(sek.GetIDString58()));

    CExtKeyAccount *sea = new CExtKeyAccount();

    {
        std::string sPath = "";
        LOCK(pwalletMain->cs_wallet);
        CWalletDB wdb(pwalletMain->strWalletFile, "r+");
        if (!wdb.TxnBegin())
            throw std::runtime_error("TxnBegin failed.");

        if (pwalletMain->ExtKeyDeriveNewAccount(&wdb, sea, sek.sLabel, sPath) != 0)
        {
            wdb.TxnAbort();
            result.insert("error_msg", "ExtKeyDeriveNewAccount failed!");
        } else
            if (!wdb.TxnCommit())
            {
                result.insert("error_msg", "TxnCommit failed!");
                emit extKeyImportResult(result);
                disconnect(this, SIGNAL(extKeyImportResult(QVariantMap)), this, SIGNAL(importFromMnemonicResult(QVariantMap)));
                return;
            };
    } // cs_wallet

    extKeySetDefault(QString::fromStdString(sea->GetIDString58()));

    newAddress(inLabel + (inLabel.isEmpty() ? "" : " ") + "default",         AT_Normal,  "", false);
    newAddress(inLabel + (inLabel.isEmpty() ? "" : " ") + "default Stealth", AT_Stealth, "", false);

    // If we get here all went well and the message is valid
    result.insert("error_msg", "");

    emit extKeyImportResult(result);
    disconnect(this, SIGNAL(extKeyImportResult(QVariantMap)), this, SIGNAL(importFromMnemonicResult(QVariantMap)));
    return;
}

void SpectreBridge::extKeySetDefault(QString extKeyID)
{
    QVariantMap result;

    std::string sInKey = extKeyID.toStdString();
    if (extKeyID.length() == 0)
    {
        result.insert("error_msg", "Must specify ext key or id.");
        emit extKeySetDefaultResult(result);
        return;
    };

    CKeyID idNewDefault;
    CBitcoinAddress addr;

    CExtKeyAccount *sea = new CExtKeyAccount();

    addr.SetString(sInKey),
    addr.IsValid(CChainParams::EXT_ACC_HASH),
    addr.GetKeyID(idNewDefault, CChainParams::EXT_ACC_HASH);

    {
        LOCK(pwalletMain->cs_wallet);
        CWalletDB wdb(pwalletMain->strWalletFile, "r+");
        if (!wdb.TxnBegin())
        {
            result.insert("error_msg", "TxnBegin failed.");
            emit extKeySetDefaultResult(result);
            return;
        }

        if (!wdb.ReadExtAccount(idNewDefault, *sea))
        {
            result.insert("error_msg", "Account not in wallet.");
            emit extKeySetDefaultResult(result);
            return;
        }

        if (!wdb.WriteNamedExtKeyId("defaultAccount", idNewDefault))
        {
            wdb.TxnAbort();
            result.insert("error_msg", "WriteNamedExtKeyId failed.");
            emit extKeySetDefaultResult(result);
            return;
        };
        if (!wdb.TxnCommit())
        {
            result.insert("error_msg", "TxnCommit failed.");
            emit extKeySetDefaultResult(result);
            return;
        }

        pwalletMain->idDefaultAccount = idNewDefault;

        // TODO: necessary?
        ExtKeyAccountMap::iterator mi = pwalletMain->mapExtAccounts.find(idNewDefault);
        if (mi == pwalletMain->mapExtAccounts.end())
            pwalletMain->mapExtAccounts[idNewDefault] = sea;
        else
            delete sea;

        result.insert("result", "Success.");
    } // cs_wallet

    // If we get here all went well
    result.insert("error_msg", "");
    emit extKeySetDefaultResult(result);
    return;
}

void SpectreBridge::extKeySetMaster(QString extKeyID)
{
    QVariantMap result;
    std::string sInKey = extKeyID.toStdString();
    if (extKeyID.length() == 0)
    {
        result.insert("error_msg", "Must specify ext key or id.");
        emit extKeySetMasterResult(result);
        return;
    };

    CKeyID idNewMaster;
    CExtKey58 eKey58;
    CExtKeyPair ekp;
    CBitcoinAddress addr;

    if (addr.SetString(sInKey)
        && addr.IsValid(CChainParams::EXT_KEY_HASH)
        && addr.GetKeyID(idNewMaster, CChainParams::EXT_KEY_HASH))
    {
        // idNewMaster is set
    } else
    if (eKey58.Set58(sInKey.c_str()) == 0)
    {
        ekp = eKey58.GetKey();
        idNewMaster = ekp.GetID();
    } else
    {
        result.insert("error_msg", "Invalid key: " + extKeyID);
        emit extKeySetMasterResult(result);
        return;
    };

    {
        LOCK(pwalletMain->cs_wallet);
        CWalletDB wdb(pwalletMain->strWalletFile, "r+");
        if (!wdb.TxnBegin())
        {
            result.insert("error_msg", "TxnBegin failed.");
            emit extKeySetMasterResult(result);
            return;
        }

        int rv;
        if (0 != (rv = pwalletMain->ExtKeySetMaster(&wdb, idNewMaster)))
        {
            wdb.TxnAbort();
            result.insert("error_msg", QString::fromStdString(strprintf("ExtKeySetMaster failed, %s.", ExtKeyGetString(rv))));
            emit extKeySetMasterResult(result);
            return;
        };
        if (!wdb.TxnCommit())
        {
            result.insert("error_msg", "TxnCommit failed.");
            emit extKeySetMasterResult(result);
            return;
        }
    } // cs_wallet

    // If we get here all went well
    result.insert("error_msg", "");
    result.insert("result", "Success.");

    emit extKeySetMasterResult(result);
    return;
}

void SpectreBridge::extKeySetActive(QString extKeyID, QString isActive)
{
    QVariantMap result;
    std::string sInKey = extKeyID.toStdString();

    if (extKeyID.length() == 0)
    {
        result.insert("error_msg", "Must specify ext key or id.");
        emit extKeySetActiveResult(result);
        return;
    };


    CBitcoinAddress addr;

    CKeyID id;
    if (!addr.SetString(sInKey))
    {
        result.insert("error_msg", "Invalid key or account id.");
        emit extKeySetActiveResult(result);
        return;
    }

    bool fAccount = false;
    bool fKey = false;
    if (addr.IsValid(CChainParams::EXT_KEY_HASH)
        && addr.GetKeyID(id, CChainParams::EXT_KEY_HASH))
    {
        // id is set
        fKey = true;
    } else
    if (addr.IsValid(CChainParams::EXT_ACC_HASH)
        && addr.GetKeyID(id, CChainParams::EXT_ACC_HASH))
    {
        // id is set
        fAccount = true;
    } else
    {
        result.insert("error_msg", "Invalid key or account id.");
        emit extKeySetActiveResult(result);
        return;
    }

    CStoredExtKey sek;
    CExtKeyAccount sea;
    {
        LOCK(pwalletMain->cs_wallet);
        CWalletDB wdb(pwalletMain->strWalletFile, "r+");
        if (!wdb.TxnBegin())
        {
            result.insert("error_msg", "TxnBegin failed.");
            emit extKeySetActiveResult(result);
            return;
        }



        if (fKey)
        {
            if (wdb.ReadExtKey(id, sek))
            {
                if (isActive == "false")
                    sek.nFlags |= EAF_ACTIVE;
                else
                    sek.nFlags &= ~EAF_ACTIVE;

                if (isActive > 0
                    && !wdb.WriteExtKey(id, sek))
                {
                    wdb.TxnAbort();
                    result.insert("error_msg", "Write failed.");
                    emit extKeySetActiveResult(result);
                    return;
                };
            } else
            {
                wdb.TxnAbort();
                result.insert("error_msg", "Account not in wallet.");
                emit extKeySetActiveResult(result);
                return;
            };
        };

        if (fAccount)
        {
            if (wdb.ReadExtAccount(id, sea))
            {

                if (isActive == "false")
                    sea.nFlags |= EAF_ACTIVE;
                else
                    sea.nFlags &= ~EAF_ACTIVE;

                if (isActive > 0
                    && !wdb.WriteExtAccount(id, sea))
                {
                    wdb.TxnAbort();
                    result.insert("error_msg", "Write failed.");
                    emit extKeySetActiveResult(result);
                    return;
                };
            } else
            {
                wdb.TxnAbort();
                result.insert("error_msg", "Account not in wallet.");
                emit extKeySetActiveResult(result);
                return;
            };
        };

        if (!wdb.TxnCommit())
        {
            result.insert("error_msg", "TxnCommit failed.");
            emit extKeySetActiveResult(result);
            return;
        }

    } // cs_wallet

    // If we get here all went well
    result.insert("error_msg", "");
    result.insert("result", "Success.");
    emit extKeySetActiveResult(result);
    return;
}
