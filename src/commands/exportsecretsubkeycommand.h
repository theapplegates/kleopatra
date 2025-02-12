/*
    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "command.h"

namespace GpgME
{
class Subkey;
}

namespace Kleo
{
class ExportSecretSubkeyCommand : public Command
{
    Q_OBJECT
public:
    explicit ExportSecretSubkeyCommand(const std::vector<GpgME::Subkey> &subkeys);
    ~ExportSecretSubkeyCommand() override;

private:
    void doStart() override;
    void doCancel() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
};
}
