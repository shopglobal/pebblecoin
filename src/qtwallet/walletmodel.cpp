// Copyright (c) 2011-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletmodel.h"

#include "addresstablemodel.h"
#include "guiconstants.h"
#include "recentrequeststablemodel.h"
#include "transactiontablemodel.h"

#include "interface/base58.h"
//#include "db.h"
//#include "keystore.h"
#include "interface/main.h"
#include "bitcoin/sync.h"
#include "interface/wallet.h"
#include "bitcoin/util.h"
//#include "walletdb.h" // for BackupWallet
#include "interface/placeholders.h"

#include "common/ui_interface.h"
#include "cryptonote_core/cryptonote_core.h"

#include <stdint.h>

#include <QDebug>
#include <QSet>
#include <QTimer>

WalletModel::WalletModel(CWallet *wallet, OptionsModel *optionsModel, QObject *parent) :
    QObject(parent), wallet(wallet), optionsModel(optionsModel), addressTableModel(0),
    transactionTableModel(0),
    recentRequestsTableModel(0),
    cachedBalance(0), cachedUnconfirmedBalance(0), cachedImmatureBalance(0),
    cachedNumTransactions(0),
    cachedEncryptionStatus(Unencrypted),
    cachedNumBlocks(0)
{
    addressTableModel = new AddressTableModel(wallet, this);
    transactionTableModel = new TransactionTableModel(wallet, this);
    recentRequestsTableModel = new RecentRequestsTableModel(wallet, this);

    // This timer will be fired repeatedly to update the balance
    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(pollBalanceChanged()));
    pollTimer->start(MODEL_UPDATE_DELAY);

    subscribeToCoreSignals();
}

WalletModel::~WalletModel()
{
    unsubscribeFromCoreSignals();
}

qint64 WalletModel::getBalance(const CCoinControl *coinControl) const
{
    return wallet->GetWallet2()->unlocked_balance();
}

qint64 WalletModel::getUnconfirmedBalance() const
{
    return wallet->GetWallet2()->balance() - wallet->GetWallet2()->unlocked_balance();
}

qint64 WalletModel::getImmatureBalance() const
{
    return 0; //wallet->GetWallet2()->balance() - wallet->GetWallet2()->unlocked_balance();
}

std::string WalletModel::getPublicAddress() const
{
    if (!wallet->HasWallet2())
        return "";
    
    return cryptonote::get_account_address_as_str(wallet->GetWallet2()->get_public_address());
}

int WalletModel::getNumTransactions() const
{
    /*int numTransactions = 0;
    {
        LOCK(wallet->cs_wallet);
        // the size of mapWallet contains the number of unique transaction IDs
        // (e.g. payments to yourself generate 2 transactions, but both share the same transaction ID)
        numTransactions = wallet->mapWallet.size();
    }
    return numTransactions;*/
    return wallet->GetWallet2()->get_num_transfers();
}

void WalletModel::updateStatus()
{
    EncryptionStatus newEncryptionStatus = getEncryptionStatus();

    if(cachedEncryptionStatus != newEncryptionStatus)
        emit encryptionStatusChanged(newEncryptionStatus);
}

void WalletModel::pollBalanceChanged()
{
    // Get required locks upfront. This avoids the GUI from getting stuck on
    // periodical polls if the core is holding the locks for a longer time -
    // for example, during a wallet rescan.
    TRY_LOCK(cs_main, lockMain);
    if(!lockMain)
        return;
    if (!pcore)
        return;
    
    TRY_LOCK(wallet->cs_wallet, lockWallet);
    if(!lockWallet)
        return;
  
    uint64_t current_height;
    crypto::hash top_id;
    pcore->get_blockchain_top(current_height, top_id);
    
    int numBlocks = (int)current_height - 1;
    
    if (numBlocks != cachedNumBlocks)
    {
        cachedNumBlocks = numBlocks;
        
        checkBalanceChanged();
        if (transactionTableModel)
            transactionTableModel->updateConfirmations();
    }
}

void WalletModel::checkBalanceChanged()
{
    qint64 newBalance = getBalance();
    qint64 newUnconfirmedBalance = getUnconfirmedBalance();
    qint64 newImmatureBalance = getImmatureBalance();

    if(cachedBalance != newBalance || cachedUnconfirmedBalance != newUnconfirmedBalance || cachedImmatureBalance != newImmatureBalance)
    {
        cachedBalance = newBalance;
        cachedUnconfirmedBalance = newUnconfirmedBalance;
        cachedImmatureBalance = newImmatureBalance;
        emit balanceChanged(newBalance, newUnconfirmedBalance, newImmatureBalance);
    }
}

void WalletModel::updateTransaction(const QString &hash, int status)
{
    if(transactionTableModel)
        transactionTableModel->updateTransaction(hash, status);

    // Balance and number of transactions might have changed
    checkBalanceChanged();

    int newNumTransactions = getNumTransactions();
    if(cachedNumTransactions != newNumTransactions)
    {
        cachedNumTransactions = newNumTransactions;
        emit numTransactionsChanged(newNumTransactions);
    }
}

void WalletModel::updateAddressBook(const QString &address, const QString &label,
        bool isMine, const QString &purpose, int status)
{
    if(addressTableModel)
        addressTableModel->updateEntry(address, label, isMine, purpose, status);
}

bool WalletModel::validateAddress(const QString &address)
{
    cryptonote::account_public_address addr;
    return cryptonote::get_account_address_from_str(addr, address.toStdString());
}

WalletModel::SendCoinsReturn WalletModel::prepareTransaction(WalletModelTransaction &transaction, const CCoinControl *coinControl)
{
    qint64 total = 0;
    const QList<SendCoinsRecipient> &recipients = transaction.getRecipients();
    //std::vector<std::pair<CScript, int64_t> > vecSend;

    if(recipients.empty())
    {
        return OK;
    }

    QSet<QString> setAddress; // Used to detect duplicates
    int nAddresses = 0;

    // Pre-check input data for validity
    foreach(const SendCoinsRecipient &rcp, recipients)
    {
        if (rcp.paymentRequest.IsInitialized())
        {/*   // PaymentRequest...
            int64_t subtotal = 0;
            const payments::PaymentDetails& details = rcp.paymentRequest.getDetails();
            for (int i = 0; i < details.outputs_size(); i++)
            {
                const payments::Output& out = details.outputs(i);
                if (out.amount() <= 0) continue;
                subtotal += out.amount();
                const unsigned char* scriptStr = (const unsigned char*)out.script().data();
                CScript scriptPubKey(scriptStr, scriptStr+out.script().size());
                vecSend.push_back(std::pair<CScript, int64_t>(scriptPubKey, out.amount()));
            }
            if (subtotal <= 0)
            {
                return InvalidAmount;
            }
            total += subtotal;*/
            return NotYetImplemented;
        }
        else
        {   // User-entered bitcoin address / amount:
            if(!validateAddress(rcp.address))
            {
                return InvalidAddress;
            }
            if(rcp.amount <= 0)
            {
                return InvalidAmount;
            }
            setAddress.insert(rcp.address);
            ++nAddresses;

            /*CScript scriptPubKey;
            scriptPubKey.SetDestination(CBitcoinAddress(rcp.address.toStdString()).Get());
            vecSend.push_back(std::pair<CScript, int64_t>(scriptPubKey, rcp.amount));*/

            total += rcp.amount;
        }
    }
    if(setAddress.size() != nAddresses)
    {
        return DuplicateAddress;
    }

    qint64 nBalance = getBalance(coinControl);

    if(total > nBalance)
    {
        return AmountExceedsBalance;
    }

    if((total + nTransactionFee) > nBalance)
    {
        transaction.setTransactionFee(nTransactionFee);
        return SendCoinsReturn(AmountWithFeeExceedsBalance);
    }
    
    int64_t nFeeRequired = DEFAULT_FEE;
    transaction.setTransactionFee(nFeeRequired);

    /*{
        LOCK2(cs_main, wallet->cs_wallet);

        transaction.newPossibleKeyChange(wallet);
        int64_t nFeeRequired = 0;
        std::string strFailReason;

        CWalletTx *newTx = transaction.getTransaction();
        CReserveKey *keyChange = transaction.getPossibleKeyChange();
        bool fCreated = wallet->CreateTransaction(vecSend, *newTx, *keyChange, nFeeRequired, strFailReason, coinControl);
        transaction.setTransactionFee(nFeeRequired);

        if(!fCreated)
        {
            if((total + nFeeRequired) > nBalance)
            {
                return SendCoinsReturn(AmountWithFeeExceedsBalance);
            }
            emit message(tr("Send Coins"), QString::fromStdString(strFailReason),
                         CClientUIInterface::MSG_ERROR);
            return TransactionCreationFailed;
        }
    }*/

    return SendCoinsReturn(OK);
}

WalletModel::SendCoinsReturn WalletModel::sendCoins(WalletModelTransaction &transaction)
{
    std::vector<cryptonote::tx_destination_entry> dsts;
    
    const QList<SendCoinsRecipient> &recipients = transaction.getRecipients();
    
    foreach(const SendCoinsRecipient &rcp, recipients)
    {
        if (rcp.paymentRequest.IsInitialized())
            return NotYetImplemented;
        
        cryptonote::tx_destination_entry de;
        if(!get_account_address_from_str(de.addr, rcp.address.toStdString()))
            return InvalidAddress;
        
        de.amount = rcp.amount;
        
        if (0 == de.amount)
            return InvalidAmount;
        
        dsts.push_back(de);
    }
    
    std::vector<uint8_t> extra;
    
    LOG_PRINT_GREEN("Fee is " << transaction.getTransactionFee() << ", default fee is " << DEFAULT_FEE << ", nTransactionFee is " << nTransactionFee, LOG_LEVEL_0);
    
    for (int fake_outs_count = 5; fake_outs_count >= 0; fake_outs_count--) {
        try
        {
            cryptonote::transaction tx;
            pwalletMain->GetWallet2()->transfer(dsts, fake_outs_count, 0, transaction.getTransactionFee(), extra, tx);
            checkBalanceChanged();
            return OK;
        }
        catch (const tools::error::daemon_busy&)
        {
            //fail_msg_writer() << "daemon is busy. Please try later";
            return TransactionCommitFailed;
        }
        catch (const tools::error::no_connection_to_daemon&)
        {
            //fail_msg_writer() << "no connection to daemon. Please, make sure daemon is running.";
            return TransactionCommitFailed;
        }
        catch (const tools::error::wallet_rpc_error& e)
        {
            //LOG_ERROR("Unknown RPC error: " << e.to_string());
            //fail_msg_writer() << "RPC error \"" << e.what() << '"';
            return TransactionCommitFailed;
        }
        catch (const tools::error::get_random_outs_error&)
        {
            //fail_msg_writer() << "failed to get random outputs to mix";
            return TransactionCreationFailed;
        }
        catch (const tools::error::not_enough_money& e)
        {
            //fail_msg_writer() << "not enough money to transfer, available only " << print_money(e.available()) << ", transaction amount " << print_money(e.tx_amount() + e.fee()) << " = " << print_money(e.tx_amount()) << " + " << print_money(e.fee()) << " (fee)";
            return AmountWithFeeExceedsBalance;
        }
        catch (const tools::error::not_enough_outs_to_mix& e)
        {
            LOG_PRINT_L0("Can't mix with " << fake_outs_count << " outs, trying 1 less...");
            continue;
            /*auto writer = fail_msg_writer();
            writer << "not enough outputs for specified mixin_count = " << e.mixin_count() << ":";
            for (const cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount& outs_for_amount : e.scanty_outs())
            {
                writer << "\noutput amount = " << print_money(outs_for_amount.amount) << ", fount outputs to mix = " << outs_for_amount.outs.size();
            }*/
            //return TransactionCreationFailed;
        }
        catch (const tools::error::tx_not_constructed&)
        {
            //fail_msg_writer() << "transaction was not constructed";
            return TransactionCreationFailed;
        }
        catch (const tools::error::tx_rejected& e)
        {
            //fail_msg_writer() << "transaction " << get_transaction_hash(e.tx()) << " was rejected by daemon with status \"" << e.status() << '"';
            return TransactionCommitFailed;
        }
        catch (const tools::error::tx_sum_overflow& e)
        {
            //fail_msg_writer() << e.what();
            return InvalidAmount;
        }
        catch (const tools::error::tx_too_big& e)
        {
            //cryptonote::transaction tx = e.tx();
            //fail_msg_writer() << "transaction " << get_transaction_hash(e.tx()) << " is too big. Transaction size: " << get_object_blobsize(e.tx()) << " bytes, transaction size limit: " << e.tx_size_limit() << " bytes";
            return TransactionCreationFailed;
        }
        catch (const tools::error::zero_destination&)
        {
            //fail_msg_writer() << "one of destinations is zero";
            return TransactionCreationFailed;
        }
        catch (const tools::error::transfer_error& e)
        {
            //LOG_ERROR("unknown transfer error: " << e.to_string());
            //fail_msg_writer() << "unknown transfer error: " << e.what();
            return TransactionCommitFailed;
        }
        catch (const tools::error::wallet_internal_error& e)
        {
            //LOG_ERROR("internal error: " << e.to_string());
            //fail_msg_writer() << "internal error: " << e.what();
            return TransactionCommitFailed;
        }
        catch (const std::exception& e)
        {
            //LOG_ERROR("unexpected error: " << e.what());
            //fail_msg_writer() << "unexpected error: " << e.what();
            return TransactionCommitFailed;
        }
        catch (...)
        {
            //LOG_ERROR("Unknown error");
            //fail_msg_writer() << "unknown error";
            return TransactionCommitFailed;
        }
    }

    return TransactionCreationFailed;
}

OptionsModel *WalletModel::getOptionsModel()
{
    return optionsModel;
}

AddressTableModel *WalletModel::getAddressTableModel()
{
    return addressTableModel;
}

TransactionTableModel *WalletModel::getTransactionTableModel()
{
    return transactionTableModel;
}

RecentRequestsTableModel *WalletModel::getRecentRequestsTableModel()
{
    return recentRequestsTableModel;
}

WalletModel::EncryptionStatus WalletModel::getEncryptionStatus() const
{
    /*if(!wallet->IsCrypted())
    {
        return Unencrypted;
    }
    else if(wallet->IsLocked())
    {
        return Locked;
    }
    else
    {
        return Unlocked;
    }*/
    return Unencrypted;
}

bool WalletModel::setWalletEncrypted(bool encrypted, const SecureString &passphrase)
{
    /*if(encrypted)
    {
        // Encrypt
        return wallet->EncryptWallet(passphrase);
    }
    else
    {
        // Decrypt -- TODO; not supported yet
        return false;
    }*/
    return false;
}

bool WalletModel::setWalletLocked(bool locked, const SecureString &passPhrase)
{
    /*if(locked)
    {
        // Lock
        return wallet->Lock();
    }
    else
    {
        // Unlock
        return wallet->Unlock(passPhrase);
    }*/
    return false;
}

bool WalletModel::changePassphrase(const SecureString &oldPass, const SecureString &newPass)
{
    /*bool retval;
    {
        LOCK(wallet->cs_wallet);
        wallet->Lock(); // Make sure wallet is locked before attempting pass change
        retval = wallet->ChangeWalletPassphrase(oldPass, newPass);
    }
    return retval;*/
    return false;
}

bool WalletModel::backupWallet(const QString &filename)
{
    return false; //BackupWallet(*wallet, filename.toLocal8Bit().data());
}

// Handlers for core signals
/*static void NotifyKeyStoreStatusChanged(WalletModel *walletmodel, CCryptoKeyStore *wallet)
{
    qDebug() << "NotifyKeyStoreStatusChanged";
    QMetaObject::invokeMethod(walletmodel, "updateStatus", Qt::QueuedConnection);
}*/

static void NotifyAddressBookChanged(WalletModel *walletmodel, CWallet *wallet,
        const address_t &address, const std::string &label, bool isMine,
        const std::string &purpose, ChangeType status)
{
    QString strAddress = QString::fromStdString(get_account_address_as_str(address));
    QString strLabel = QString::fromStdString(label);
    QString strPurpose = QString::fromStdString(purpose);

    qDebug() << "NotifyAddressBookChanged : " + strAddress + " " + strLabel + " isMine=" + QString::number(isMine) + " purpose=" + strPurpose + " status=" + QString::number(status);
    QMetaObject::invokeMethod(walletmodel, "updateAddressBook", Qt::QueuedConnection,
                              Q_ARG(QString, strAddress),
                              Q_ARG(QString, strLabel),
                              Q_ARG(bool, isMine),
                              Q_ARG(QString, strPurpose),
                              Q_ARG(int, status));
}

// queue notifications to show a non freezing progress dialog e.g. for rescan
//static bool fQueueNotifications = false;
//static std::vector<std::pair<std::string, ChangeType> > vQueueNotifications;
static void NotifyTransactionChanged(WalletModel *walletmodel, CWallet *wallet, const std::string &hash, ChangeType status)
{
    /*if (fQueueNotifications)
    {
        vQueueNotifications.push_back(std::make_pair(hash, status));
        return;
    }*/

    QString strHash = QString::fromStdString(hash); //.GetHex());

    LOG_PRINT_L2("NotifyTransactionChanged : " << hash << " status= " << status);
    QMetaObject::invokeMethod(walletmodel, "updateTransaction", Qt::QueuedConnection,
                              Q_ARG(QString, strHash),
                              Q_ARG(int, status));
}
/*
static void ShowProgress(WalletModel *walletmodel, const std::string &title, int nProgress)
{
    // emits signal "showProgress"
    QMetaObject::invokeMethod(walletmodel, "showProgress", Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromStdString(title)),
                              Q_ARG(int, nProgress));

    if (nProgress == 0)
        fQueueNotifications = true;

    if (nProgress == 100)
    {
        fQueueNotifications = false;
        BOOST_FOREACH(const PAIRTYPE(std::string, ChangeType)& notification, vQueueNotifications)
            NotifyTransactionChanged(walletmodel, NULL, notification.first, notification.second);
        std::vector<std::pair<std::string, ChangeType> >().swap(vQueueNotifications); // clear
    }
}*/

void WalletModel::subscribeToCoreSignals()
{
    // Connect signals to wallet
    //wallet->NotifyStatusChanged.connect(boost::bind(&NotifyKeyStoreStatusChanged, this, _1));
    wallet->NotifyAddressBookChanged.connect(boost::bind(NotifyAddressBookChanged, this, _1, _2, _3, _4, _5, _6));
    wallet->NotifyTransactionChanged.connect(boost::bind(NotifyTransactionChanged, this, _1, _2, _3));
    //wallet->ShowProgress.connect(boost::bind(ShowProgress, this, _1, _2));
}

void WalletModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from wallet
    //wallet->NotifyStatusChanged.disconnect(boost::bind(&NotifyKeyStoreStatusChanged, this, _1));
    wallet->NotifyAddressBookChanged.disconnect(boost::bind(NotifyAddressBookChanged, this, _1, _2, _3, _4, _5, _6));
    wallet->NotifyTransactionChanged.disconnect(boost::bind(NotifyTransactionChanged, this, _1, _2, _3));
    //wallet->ShowProgress.disconnect(boost::bind(ShowProgress, this, _1, _2));
}

// WalletModel::UnlockContext implementation
WalletModel::UnlockContext WalletModel::requestUnlock()
{
    bool was_locked = getEncryptionStatus() == Locked;
    if(was_locked)
    {
        // Request UI to unlock wallet
        emit requireUnlock();
    }
    // If wallet is still locked, unlock was failed or cancelled, mark context as invalid
    bool valid = getEncryptionStatus() != Locked;

    return UnlockContext(this, valid, was_locked);
}

WalletModel::UnlockContext::UnlockContext(WalletModel *wallet, bool valid, bool relock):
        wallet(wallet),
        valid(valid),
        relock(relock)
{
}

WalletModel::UnlockContext::~UnlockContext()
{
    if(valid && relock)
    {
        wallet->setWalletLocked(true);
    }
}

void WalletModel::UnlockContext::CopyFrom(const UnlockContext& rhs)
{
    // Transfer context; old object no longer relocks wallet
    *this = rhs;
    rhs.relock = false;
}

bool WalletModel::getPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const
{
    return false; //wallet->GetPubKey(address, vchPubKeyOut);
}

// returns a list of COutputs from COutPoints
void WalletModel::getOutputs(const std::vector<COutPoint>& vOutpoints, std::vector<COutput>& vOutputs)
{
    //LOCK2(cs_main, wallet->cs_wallet);
    /*BOOST_FOREACH(const COutPoint& outpoint, vOutpoints)
    {
        if (!wallet->mapWallet.count(outpoint.hash)) continue;
        int nDepth = wallet->mapWallet[outpoint.hash].GetDepthInMainChain();
        if (nDepth < 0) continue;
        COutput out(&wallet->mapWallet[outpoint.hash], outpoint.n, nDepth);
        vOutputs.push_back(out);
    }*/
}

bool WalletModel::isSpent(const COutPoint& outpoint) const
{
    //LOCK2(cs_main, wallet->cs_wallet);
    return false; //wallet->IsSpent(outpoint.hash, outpoint.n);
}

// AvailableCoins + LockedCoins grouped by wallet address (put change in one group with wallet address)
void WalletModel::listCoins(std::map<QString, std::vector<COutput> >& mapCoins) const
{
    /*std::vector<COutput> vCoins;
    wallet->AvailableCoins(vCoins);

    LOCK2(cs_main, wallet->cs_wallet); // ListLockedCoins, mapWallet
    std::vector<COutPoint> vLockedCoins;
    wallet->ListLockedCoins(vLockedCoins);

    // add locked coins
    BOOST_FOREACH(const COutPoint& outpoint, vLockedCoins)
    {
        if (!wallet->mapWallet.count(outpoint.hash)) continue;
        int nDepth = wallet->mapWallet[outpoint.hash].GetDepthInMainChain();
        if (nDepth < 0) continue;
        COutput out(&wallet->mapWallet[outpoint.hash], outpoint.n, nDepth);
        vCoins.push_back(out);
    }

    BOOST_FOREACH(const COutput& out, vCoins)
    {
        COutput cout = out;

        while (wallet->IsChange(cout.tx->vout[cout.i]) && cout.tx->vin.size() > 0 && wallet->IsMine(cout.tx->vin[0]))
        {
            if (!wallet->mapWallet.count(cout.tx->vin[0].prevout.hash)) break;
            cout = COutput(&wallet->mapWallet[cout.tx->vin[0].prevout.hash], cout.tx->vin[0].prevout.n, 0);
        }

        CTxDestination address;
        if(!ExtractDestination(cout.tx->vout[cout.i].scriptPubKey, address)) continue;
        mapCoins[CBitcoinAddress(address).ToString().c_str()].push_back(out);
    }*/
    std::vector<COutput> lawl;
    mapCoins["NYI"] = lawl;
}

bool WalletModel::isLockedCoin(std::string hash, unsigned int n) const
{
    //LOCK2(cs_main, wallet->cs_wallet);
    return false; //wallet->IsLockedCoin(hash, n);
}

void WalletModel::lockCoin(COutPoint& output)
{
    /*LOCK2(cs_main, wallet->cs_wallet);
    wallet->LockCoin(output);*/
    return;
}

void WalletModel::unlockCoin(COutPoint& output)
{
    /*LOCK2(cs_main, wallet->cs_wallet);
    wallet->UnlockCoin(output);*/
    return;
}

void WalletModel::listLockedCoins(std::vector<COutPoint>& vOutpts)
{
    /*LOCK2(cs_main, wallet->cs_wallet);
    wallet->ListLockedCoins(vOutpts);*/
    return;
}

void WalletModel::loadReceiveRequests(std::vector<std::string>& vReceiveRequests)
{
    /*LOCK(wallet->cs_wallet);
    BOOST_FOREACH(const PAIRTYPE(CTxDestination, CAddressBookData)& item, wallet->mapAddressBook)
        BOOST_FOREACH(const PAIRTYPE(std::string, std::string)& item2, item.second.destdata)
            if (item2.first.size() > 2 && item2.first.substr(0,2) == "rr") // receive request
                vReceiveRequests.push_back(item2.second);*/
    return;
}

bool WalletModel::saveReceiveRequest(const std::string &sAddress, const int64_t nId, const std::string &sRequest)
{
    /*CTxDestination dest = CBitcoinAddress(sAddress).Get();

    std::stringstream ss;
    ss << nId;
    std::string key = "rr" + ss.str(); // "rr" prefix = "receive request" in destdata

    LOCK(wallet->cs_wallet);
    if (sRequest.empty())
        return wallet->EraseDestData(dest, key);
    else
        return wallet->AddDestData(dest, key, sRequest);*/
    return false;
}
