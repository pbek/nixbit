/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TERMINAL_INTERFACE_H
#define TERMINAL_INTERFACE_H

#include <QObject>
#include <QString>

/**
 * This interface provides access to a terminal's input/output
 */
class TerminalInterface {
public:
  virtual ~TerminalInterface() = default;

  /**
   * Sends a string of characters to the terminal.
   */
  virtual void sendInput(const QString &text) = 0;
};

Q_DECLARE_INTERFACE(TerminalInterface, "org.kde.konsole.TerminalInterface")

#endif // TERMINAL_INTERFACE_H
