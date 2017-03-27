/* -*- mode: c++; c-basic-offset:4 -*-
    commands/signcertificatecommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2008 Klarälvdalens Datakonsult AB

    Kleopatra is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kleopatra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Softwarls   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    In addition, as a special exception, the copyright holders give
    permission to link the code of this program with any edition of
    the Qt library by Trolltech AS, Norway (or with modified versions
    of Qt that use the same license as Qt), and distribute linked
    combinations including the two.  You must obey the GNU General
    Public License in all respects for all of the code used other than
    Qt.  If you modify this file, you may extend this exception to
    your version of the file, but you are not obligated to do so.  If
    you do not wish to do so, delete this exception statement from
    your version.
*/

#include <config-kleopatra.h>

#include "certifycertificatecommand.h"
#include "newcertificatecommand.h"

#include "command_p.h"

#include "exportopenpgpcertstoservercommand.h"

#include <dialogs/certifycertificatedialog.h>

#include <Libkleo/KeyCache>
#include <Libkleo/Formatting>

#include <QGpgME/Protocol>
#include <QGpgME/SignKeyJob>

#include <QEventLoop>

#include <gpgme++/key.h>

#include <KLocalizedString>
#include "kleopatra_debug.h"

#include <cassert>

using namespace Kleo;
using namespace Kleo::Commands;
using namespace Kleo::Dialogs;
using namespace GpgME;
using namespace QGpgME;

class CertifyCertificateCommand::Private : public Command::Private
{
    friend class ::Kleo::Commands::CertifyCertificateCommand;
    CertifyCertificateCommand *q_func() const
    {
        return static_cast<CertifyCertificateCommand *>(q);
    }
public:
    explicit Private(CertifyCertificateCommand *qq, KeyListController *c);
    ~Private();

    void init();

private:
    void slotDialogRejected();
    void slotResult(const Error &err);
    void slotCertificationPrepared();

private:
    void ensureDialogCreated();
    void createJob();

private:
    std::vector<UserID> uids;
    QPointer<CertifyCertificateDialog> dialog;
    QPointer<QGpgME::SignKeyJob> job;
};

CertifyCertificateCommand::Private *CertifyCertificateCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const CertifyCertificateCommand::Private *CertifyCertificateCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define d d_func()
#define q q_func()

CertifyCertificateCommand::Private::Private(CertifyCertificateCommand *qq, KeyListController *c)
    : Command::Private(qq, c),
      uids(),
      dialog(),
      job()
{

}

CertifyCertificateCommand::Private::~Private()
{
    qCDebug(KLEOPATRA_LOG);
}

CertifyCertificateCommand::CertifyCertificateCommand(KeyListController *c)
    : Command(new Private(this, c))
{
    d->init();
}

CertifyCertificateCommand::CertifyCertificateCommand(QAbstractItemView *v, KeyListController *c)
    : Command(v, new Private(this, c))
{
    d->init();
}

CertifyCertificateCommand::CertifyCertificateCommand(const Key &key)
    : Command(key, new Private(this, nullptr))
{
    d->init();
}

CertifyCertificateCommand::CertifyCertificateCommand(const UserID &uid)
    : Command(uid.parent(), new Private(this, nullptr))
{
    std::vector<UserID>(1, uid).swap(d->uids);
    d->init();
}

CertifyCertificateCommand::CertifyCertificateCommand(const std::vector<UserID> &uids)
    : Command(uids.empty() ? Key() : uids.front().parent(), new Private(this, nullptr))
{
    d->uids = uids;
    d->init();
}

void CertifyCertificateCommand::Private::init()
{

}

CertifyCertificateCommand::~CertifyCertificateCommand()
{
    qCDebug(KLEOPATRA_LOG);
}

void CertifyCertificateCommand::setCertificationExportable(bool on)
{
    Q_UNUSED(on);
}

void CertifyCertificateCommand::setCertificationRevocable(bool on)
{
    Q_UNUSED(on);
}

void CertifyCertificateCommand::setCertifyingKey(const Key &signer)
{
    Q_UNUSED(signer);
}

void CertifyCertificateCommand::setUserIDs(const std::vector<UserID> &uids)
{
    d->uids = uids;
    if (!uids.empty() && d->key().isNull()) {
        setKey(uids.front().parent());
    }
}

void CertifyCertificateCommand::setUserID(const UserID &uid)
{
    setUserIDs(std::vector<UserID>(1, uid));
}

void CertifyCertificateCommand::doStart()
{

    const std::vector<Key> keys = d->keys();
    if (keys.size() != 1 ||
            keys.front().protocol() != GpgME::OpenPGP) {
        d->finished();
        return;
    }

    std::vector<Key> secKeys;
    Q_FOREACH (const Key &secKey, KeyCache::instance()->secretKeys()) {
        // Only include usable keys.
        if (secKey.canCertify() && secKey.protocol() == OpenPGP && !secKey.isRevoked() &&
            !secKey.isExpired() && !secKey.isInvalid()) {
            secKeys.push_back(secKey);
        }
    }

    if (secKeys.empty()) {
        auto sel = KMessageBox::questionYesNo(d->parentWidgetOrView(),
                    xi18nc("@info", "To certify other certificates, you first need to create an OpenPGP certificate for yourself.") +
                    QStringLiteral("<br><br>") +
                    i18n("Do you wish to create one now?"),
                    i18n("Certification Not Possible"));
        if (sel == KMessageBox::Yes) {
            QEventLoop loop;
            auto cmd = new Commands::NewCertificateCommand();
            cmd->setParentWidget(d->parentWidgetOrView());
            cmd->setProtocol(GpgME::OpenPGP);
            loop.connect(cmd, SIGNAL(finished()), SLOT(quit()));
            QMetaObject::invokeMethod(cmd, "start", Qt::QueuedConnection);
            loop.exec();
        } else {
            Q_EMIT(canceled());
            d->finished();
            return;
        }
    }
    Q_FOREACH (const Key &secKey, KeyCache::instance()->secretKeys()) {
        // Check again for secret keys
        if (secKey.canCertify() && secKey.protocol() == OpenPGP && !secKey.isRevoked() &&
            !secKey.isExpired() && !secKey.isInvalid()) {
            secKeys.push_back(secKey);
        }
    }
    if (secKeys.empty()) {
        qCDebug(KLEOPATRA_LOG) << "Sec Keys still empty after keygen.";
        Q_EMIT(canceled());
        d->finished();
        return;
    }
    const Key &key = keys.front();

    Q_FOREACH (const UserID &uid, d->uids)
        if (qstricmp(uid.parent().primaryFingerprint(), key.primaryFingerprint()) != 0) {
            qCWarning(KLEOPATRA_LOG) << "User-ID <-> Key mismatch!";
            d->finished();
            return;
        }

    d->ensureDialogCreated();
    assert(d->dialog);
    d->dialog->setCertificateToCertify(d->key());
    d->dialog->setSelectedUserIDs(d->uids);
    d->dialog->setCertificatesWithSecretKeys(secKeys);
    d->dialog->show();
}

void CertifyCertificateCommand::Private::slotDialogRejected()
{
    Q_EMIT q->canceled();
    finished();
}

void CertifyCertificateCommand::Private::slotResult(const Error &err)
{
    if (!err && !err.isCanceled() && dialog && dialog->exportableCertificationSelected() && dialog->sendToServer()) {
        ExportOpenPGPCertsToServerCommand *const cmd = new ExportOpenPGPCertsToServerCommand(key());
        cmd->start();
    }

    finished();
}

void CertifyCertificateCommand::Private::slotCertificationPrepared()
{
    assert(dialog);

    createJob();
    assert(job);
    job->setExportable(dialog->exportableCertificationSelected());
    job->setNonRevocable(dialog->nonRevocableCertificationSelected());
    job->setUserIDsToSign(dialog->selectedUserIDs());
    job->setSigningKey(dialog->selectedSecretKey());
    job->setCheckLevel(dialog->selectedCheckLevel());

    dialog->connectJob(job);

    if (const Error err = job->start(key())) {
        dialog->setError(err);
        finished();
    }
}

void CertifyCertificateCommand::doCancel()
{
    qCDebug(KLEOPATRA_LOG);
    if (d->job) {
        d->job->slotCancel();
    }
}

void CertifyCertificateCommand::Private::ensureDialogCreated()
{
    if (dialog) {
        return;
    }

    dialog = new CertifyCertificateDialog;
    applyWindowID(dialog);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(dialog, SIGNAL(rejected()), q, SLOT(slotDialogRejected()));
    connect(dialog, SIGNAL(certificationPrepared()), q, SLOT(slotCertificationPrepared()));
}

void CertifyCertificateCommand::Private::createJob()
{
    if (dialog) {
        disconnect(dialog, SIGNAL(certificationPrepared()), q, SLOT(slotCertificationPrepared()));
    }

    assert(!job);

    assert(key().protocol() == OpenPGP);
    const auto backend = QGpgME::openpgp();
    if (!backend) {
        return;
    }

    SignKeyJob *const j = backend->signKeyJob();
    if (!j) {
        return;
    }

    connect(j, &Job::progress,
            q, &Command::progress);
    connect(j, SIGNAL(result(GpgME::Error)),
            q, SLOT(slotResult(GpgME::Error)));

    job = j;
}

#undef d
#undef q

#include "moc_certifycertificatecommand.cpp"
