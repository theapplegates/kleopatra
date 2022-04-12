/*  utils/accessibility.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "accessibility.h"

#include <KLocalizedString>

#include <QLabel>
#include <QObject>
#include <QTextDocument>

#include <algorithm>

#include "kleopatra_debug.h"

using namespace Kleo;

namespace
{
QString getAccessibleText(QObject *object, QAccessible::Text t)
{
    QString name;
    if (const auto *const iface = QAccessible::queryAccessibleInterface(object)) {
        name = iface->text(t);
    }
    return name;
}
}

QString Kleo::getAccessibleName(QObject *object)
{
    return getAccessibleText(object, QAccessible::Name);
}

QString Kleo::getAccessibleDescription(QObject *object)
{
    return getAccessibleText(object, QAccessible::Description);
}

QString Kleo::invalidEntryText()
{
    return i18nc("text for screen readers to indicate that the associated object, "
                 "such as a form field, has an error",
                 "invalid entry");
}

QString Kleo::requiredText()
{
    return i18nc("text for screen readers to indicate that the associated object, "
                 "such as a form field must be filled out",
                 "required");
}

void Kleo::selectLabelText(QLabel *label)
{
    if (!label || label->text().isEmpty()) {
        return;
    }
    if (label->textFormat() == Qt::PlainText) {
        label->setSelection(0, label->text().size());
    } else if (label->textFormat() == Qt::RichText) {
        // unfortunately, there is no selectAll(); therefore, we need
        // to determine the "visual" length of the text by stripping
        // the label's text of all formatting information
        QTextDocument temp;
        temp.setHtml(label->text());
        label->setSelection(0, temp.toRawText().size());
    } else {
        qCDebug(KLEOPATRA_LOG) << "Label with unsupported text format" << label->textFormat() << "got focus";
    }
}

LabelHelper::LabelHelper()
{
    QAccessible::installActivationObserver(this);
}

LabelHelper::~LabelHelper()
{
    QAccessible::removeActivationObserver(this);
}

void LabelHelper::addLabel(QLabel *label)
{
    mLabels.push_back(label);
    accessibilityActiveChanged(QAccessible::isActive());
}

void LabelHelper::accessibilityActiveChanged(bool active)
{
    // Allow text labels to get focus if accessibility is active
    const auto focusPolicy = active ? Qt::StrongFocus : Qt::ClickFocus;
    std::for_each(std::cbegin(mLabels), std::cend(mLabels),
                  [focusPolicy](const auto &label) {
                      if (label) {
                          label->setFocusPolicy(focusPolicy);
                      }
                  });
}
