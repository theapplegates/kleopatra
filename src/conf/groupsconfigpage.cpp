/*
    conf/groupsconfigpage.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2021 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "groupsconfigpage.h"

#include "groupsconfigwidget.h"

#include <Libkleo/Debug>
#include <Libkleo/KeyCache>
#include <Libkleo/KeyGroup>
#include <Libkleo/UniqueLock>

#include <KLocalizedString>
#include <KMessageBox>

#include <QVBoxLayout>
#include <QMutex>

#include "kleopatra_debug.h"
#include <kwidgetsaddons_version.h>

using namespace Kleo;

class GroupsConfigPage::Private
{
    friend class ::GroupsConfigPage;
    GroupsConfigPage *const q;

    Private(GroupsConfigPage *qq);
public:
    ~Private() = default;

    void setChanged(bool changed);

    void onKeysMayHaveChanged();

private:
    GroupsConfigWidget *widget = nullptr;
    bool changed = false;
    bool savingChanges = false;
};

GroupsConfigPage::Private::Private(GroupsConfigPage *qq)
    : q(qq)
{
}

void GroupsConfigPage::Private::setChanged(bool state)
{
    changed = state;
    Q_EMIT q->changed(changed);
}

void GroupsConfigPage::Private::onKeysMayHaveChanged()
{
    static QMutex mutex;

    const UniqueLock lock{mutex, Kleo::tryToLock};
    if (!lock) {
        // prevent reentrace
        return;
    }

    if (savingChanges) {
        qCDebug(KLEOPATRA_LOG) << __func__ << "ignoring changes caused by ourselves";
        return;
    }
    if (!changed) {
        q->load();
    } else {
        auto buttonYes = KStandardGuiItem::yes();
        buttonYes.setText(i18n("Save changes"));
        auto buttonNo = KStandardGuiItem::no();
        buttonNo.setText(i18n("Discard changes"));
#if KWIDGETSADDONS_VERSION >= QT_VERSION_CHECK(5, 100, 0)
        const auto answer = KMessageBox::questionTwoActions(
#else
        const auto answer = KMessageBox::questionYesNo(
#endif
            q->topLevelWidget(),
            xi18nc("@info",
                   "<para>The certificates or the certificate groups have been updated in the background.</para>"
                   "<para>Do you want to save your changes?</para>"),
            i18nc("@title::window", "Save changes?"),
            buttonYes,
            buttonNo);
#if KWIDGETSADDONS_VERSION >= QT_VERSION_CHECK(5, 100, 0)
        if (answer == KMessageBox::ButtonCode::PrimaryAction) {
#else
        if (answer == KMessageBox::Yes) {
#endif
            q->save();
        } else {
            q->load();
        }
    }
}

GroupsConfigPage::GroupsConfigPage(QWidget *parent)
    : QWidget(parent)
    , d(new Private(this))
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    d->widget = new Kleo::GroupsConfigWidget(this);
    if (QLayout *l = d->widget->layout()) {
        l->setContentsMargins(0, 0, 0, 0);
    }

    layout->addWidget(d->widget);

    connect(d->widget, &GroupsConfigWidget::changed, this, [this] () { d->setChanged(true); });

    connect(KeyCache::instance().get(), &KeyCache::keysMayHaveChanged,
            this, [this]() { d->onKeysMayHaveChanged(); });
}

GroupsConfigPage::~GroupsConfigPage() = default;

bool GroupsConfigPage::hasChanged() const
{
    return d->changed;
}

void GroupsConfigPage::load()
{
    d->widget->setGroups(KeyCache::instance()->configurableGroups());
    d->setChanged(false);
}

void GroupsConfigPage::save()
{
    d->savingChanges = true;
    KeyCache::mutableInstance()->saveConfigurableGroups(d->widget->groups());
    d->savingChanges = false;

    // reload after saving to ensure that the groups reflect the saved groups (e.g. in case of immutable entries)
    load();
}
