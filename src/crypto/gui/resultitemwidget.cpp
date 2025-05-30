/*
    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2008 Klarälvdalens Datakonsult AB

    SPDX-FileCopyrightText: 2016 Bundesamt für Sicherheit in der Informationstechnik
    SPDX-FileContributor: Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "resultitemwidget.h"

#include "commands/command.h"
#include "commands/lookupcertificatescommand.h"
#include "crypto/decryptverifytask.h"
#include "view/htmllabel.h"
#include "view/urllabel.h"

#include <Libkleo/AuditLogEntry>
#include <Libkleo/AuditLogViewer>
#include <Libkleo/Classify>
#include <Libkleo/SystemInfo>

#include <gpgme++/decryptionresult.h>
#include <gpgme++/key.h>

#include "kleopatra_debug.h"
#include <KColorScheme>
#include <KGuiItem>
#include <KLocalizedString>
#include <KStandardGuiItem>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

using namespace Kleo;
using namespace Kleo::Crypto;
using namespace Kleo::Crypto::Gui;

namespace
{
// TODO move out of here
static QColor colorForVisualCode(Task::Result::VisualCode code)
{
    switch (code) {
    case Task::Result::AllGood:
        return KColorScheme(QPalette::Active, KColorScheme::View).background(KColorScheme::PositiveBackground).color();
    case Task::Result::Warning:
        return KColorScheme(QPalette::Active, KColorScheme::View).background(KColorScheme::NormalBackground).color();
    case Task::Result::Danger:
        return KColorScheme(QPalette::Active, KColorScheme::View).background(KColorScheme::NegativeBackground).color();
    default:
        return KColorScheme(QPalette::Active, KColorScheme::View).background(KColorScheme::NormalBackground).color();
    }
}
}

class ResultItemWidget::Private
{
    ResultItemWidget *const q;

public:
    explicit Private(const std::shared_ptr<const Task::Result> &result, ResultItemWidget *qq)
        : q(qq)
        , m_result(result)
    {
        Q_ASSERT(m_result);
    }

    void slotLinkActivated(const QString &);
    void updateShowDetailsLabel();

    void addIgnoreMDCButton(QBoxLayout *lay);

    void oneImportFinished();

    const std::shared_ptr<const Task::Result> m_result;
    UrlLabel *m_auditLogLabel = nullptr;
    QPushButton *m_closeButton = nullptr;
    QPushButton *m_showButton = nullptr;
    bool m_importCanceled = false;
};

void ResultItemWidget::Private::oneImportFinished()
{
    if (m_importCanceled) {
        return;
    }
    if (m_result->parentTask()) {
        m_result->parentTask()->start();
    }
    q->setVisible(false);
}

void ResultItemWidget::Private::addIgnoreMDCButton(QBoxLayout *lay)
{
    if (!m_result || !lay) {
        return;
    }

    const auto dvResult = dynamic_cast<const DecryptVerifyResult *>(m_result.get());
    if (!dvResult) {
        return;
    }
    const auto decResult = dvResult->decryptionResult();

    if (decResult.isNull() || !decResult.error() || !decResult.isLegacyCipherNoMDC()) {
        return;
    }

    auto btn = new QPushButton(i18nc("@action:button", "Force decryption"));
    btn->setFixedSize(btn->sizeHint());

    connect(btn, &QPushButton::clicked, q, [this]() {
        if (m_result->parentTask()) {
            const auto dvTask = dynamic_cast<DecryptVerifyTask *>(m_result->parentTask().data());
            dvTask->setIgnoreMDCError(true);
            dvTask->start();
            q->setVisible(false);
        } else {
            qCWarning(KLEOPATRA_LOG) << "Failed to get parent task";
        }
    });
    lay->addWidget(btn);
}

static QUrl auditlog_url_template()
{
    QUrl url(QStringLiteral("kleoresultitem://showauditlog"));
    return url;
}

void ResultItemWidget::Private::updateShowDetailsLabel()
{
    const auto auditLogUrl = m_result->auditLog().asUrl(auditlog_url_template());
    const auto auditLogLinkText = m_result->hasError() ? i18n("Diagnostics") //
                                                       : i18nc("The Audit Log is a detailed error log from the gnupg backend", "Show Audit Log");
    m_auditLogLabel->setUrl(auditLogUrl, auditLogLinkText);
    m_auditLogLabel->setVisible(!auditLogUrl.isEmpty());
}

ResultItemWidget::ResultItemWidget(const std::shared_ptr<const Task::Result> &result, QWidget *parent, Qt::WindowFlags flags)
    : QWidget(parent, flags)
    , d(new Private(result, this))
{
    const auto color = KColorScheme(QPalette::Active, KColorScheme::View).background(KColorScheme::NormalBackground).color();
    const QString styleSheet = SystemInfo::isHighContrastModeActive()
        ? QStringLiteral(
              "QFrame,QLabel { margin: 0px; }"
              "QFrame#resultFrame{ border-style: solid; border-radius: 3px; border-width: 1px }"
              "QLabel { padding: 5px; border-radius: 3px }")
        : QStringLiteral(
              "QFrame,QLabel { background-color: %1; margin: 0px; }"
              "QFrame#resultFrame{ border-color: %2; border-style: solid; border-radius: 3px; border-width: 1px }"
              "QLabel { padding: 5px; border-radius: 3px }")
              .arg(color.name())
              .arg(color.darker(150).name());
    auto topLayout = new QVBoxLayout(this);
    auto frame = new QFrame;
    frame->setObjectName(QLatin1StringView("resultFrame"));
    frame->setStyleSheet(styleSheet);
    topLayout->addWidget(frame);
    auto vlay = new QVBoxLayout(frame);
    auto layout = new QHBoxLayout();
    auto overview = new HtmlLabel;
    overview->setWordWrap(true);
    overview->setHtml(d->m_result->overview());
    overview->setStyleSheet(styleSheet);
    setFocusPolicy(overview->focusPolicy());
    setFocusProxy(overview);
    connect(overview, &QLabel::linkActivated, this, [this](const auto &link) {
        d->slotLinkActivated(link);
    });

    layout->addWidget(overview);
    vlay->addLayout(layout);

    auto actionLayout = new QVBoxLayout;
    layout->addLayout(actionLayout);

    d->addIgnoreMDCButton(actionLayout);

    d->m_auditLogLabel = new UrlLabel;
    connect(d->m_auditLogLabel, &QLabel::linkActivated, this, [this](const auto &link) {
        d->slotLinkActivated(link);
    });
    actionLayout->addWidget(d->m_auditLogLabel);
    d->m_auditLogLabel->setStyleSheet(styleSheet);

    for (const auto &detail : d->m_result.get()->detailsList()) {
        auto frame = new QFrame;
        auto row = new QHBoxLayout(frame);

        auto iconLabel = new QLabel;
        QString icon;
        if (detail.code == Task::Result::AllGood) {
            icon = QStringLiteral("data-success");
        } else if (detail.code == Task::Result::Warning) {
            icon = QStringLiteral("data-warning");
        } else {
            icon = QStringLiteral("data-error");
        }
        iconLabel->setPixmap(QIcon::fromTheme(icon).pixmap(32, 32));
        row->addWidget(iconLabel, 0);

        auto detailsLabel = new HtmlLabel;
        detailsLabel->setWordWrap(true);
        detailsLabel->setHtml(detail.details);
        iconLabel->setStyleSheet(QStringLiteral("QLabel {border-width: 0; }"));
        auto color = colorForVisualCode(detail.code);

        const QString styleSheet = SystemInfo::isHighContrastModeActive()
            ? QStringLiteral(
                  "QFrame { margin: 0px; }"
                  "QFrame { padding: 5px; border-radius: 3px }")
            : QStringLiteral(
                  "QFrame { background-color: %1; margin: 0px; }"
                  "QFrame { padding: 5px; border-radius: 3px; border-style: solid; border-width: 1px; border-color: %2; }")
                  .arg(color.name())
                  .arg(color.darker(150).name());
        frame->setStyleSheet(styleSheet);
        detailsLabel->setStyleSheet(QStringLiteral("QLabel {border-width: 0; }"));
        connect(detailsLabel, &QLabel::linkActivated, this, [this](const auto &link) {
            d->slotLinkActivated(link);
        });

        row->addWidget(detailsLabel, 1);
        vlay->addWidget(frame);
    }

    d->m_showButton = new QPushButton;
    d->m_showButton->setVisible(false);
    connect(d->m_showButton, &QAbstractButton::clicked, this, &ResultItemWidget::showButtonClicked);
    actionLayout->addWidget(d->m_showButton);

    d->m_closeButton = new QPushButton;
    KGuiItem::assign(d->m_closeButton, KStandardGuiItem::close());
    d->m_closeButton->setFixedSize(d->m_closeButton->sizeHint());
    connect(d->m_closeButton, &QAbstractButton::clicked, this, &ResultItemWidget::closeButtonClicked);
    actionLayout->addWidget(d->m_closeButton);
    d->m_closeButton->setVisible(false);

    layout->setStretch(0, 1);
    actionLayout->addStretch(-1);
    vlay->addStretch(-1);

    d->updateShowDetailsLabel();
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);
}

ResultItemWidget::~ResultItemWidget()
{
}

void ResultItemWidget::showCloseButton(bool show)
{
    d->m_closeButton->setVisible(show);
}

void ResultItemWidget::setShowButton(const QString &text, bool show)
{
    d->m_showButton->setText(text);
    d->m_showButton->setVisible(show);
}

bool ResultItemWidget::hasErrorResult() const
{
    return d->m_result->hasError();
}

void ResultItemWidget::Private::slotLinkActivated(const QString &link)
{
    Q_ASSERT(m_result);
    qCDebug(KLEOPATRA_LOG) << "Link activated: " << link;
    if (link.startsWith(QLatin1StringView("key:"))) {
        auto split = link.split(QLatin1Char(':'));
        auto fpr = split.value(1);
        if (split.size() == 2 && isFingerprint(fpr)) {
            /* There might be a security consideration here if somehow
             * a short keyid is used in a link and it collides with another.
             * So we additionally check that it really is a fingerprint. */
            auto cmd = Command::commandForQuery(fpr);
            cmd->setParentWId(q->effectiveWinId());
            cmd->start();
        } else {
            qCWarning(KLEOPATRA_LOG) << "key link invalid " << link;
        }
        return;
    }

    const QUrl url(link);

    if (url.host() == QLatin1StringView("showauditlog")) {
        q->showAuditLog();
        return;
    }

    if (url.scheme() == QStringLiteral("certificate")) {
        auto cmd = new Kleo::Commands::LookupCertificatesCommand(url.path(), nullptr);
        connect(cmd, &Kleo::Commands::LookupCertificatesCommand::canceled, q, [this]() {
            m_importCanceled = true;
        });
        connect(cmd, &Kleo::Commands::LookupCertificatesCommand::finished, q, [this]() {
            oneImportFinished();
        });
        cmd->setParentWidget(q);
        cmd->start();
        return;
    }
    qCWarning(KLEOPATRA_LOG) << "Unexpected link scheme: " << link;
}

void ResultItemWidget::showAuditLog()
{
    AuditLogViewer::showAuditLog(parentWidget(), d->m_result->auditLog());
}

#include "moc_resultitemwidget.cpp"
