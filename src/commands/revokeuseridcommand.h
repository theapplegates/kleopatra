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
class UserID;
}

namespace Kleo
{
namespace Commands
{

class RevokeUserIDCommand : public Command
{
    Q_OBJECT
public:
    explicit RevokeUserIDCommand(const GpgME::UserID &userId);
    ~RevokeUserIDCommand() override;

private:
    void doStart() override;
    void doCancel() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
};

}
}
