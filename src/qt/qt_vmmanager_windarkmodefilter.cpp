/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Generic Windows native event filter for dark mode handling
 *
 * Authors: Teemu Korhonen
 *          Cacodemon345
 *
 *          Copyright 2021 Teemu Korhonen
 *          Copyright 2024-2025 Cacodemon345.
 */
#include "qt_vmmanager_windarkmodefilter.hpp"

#include <QTimer>

#include <windows.h>

#include <86box/86box.h>
#include <86box/plat.h>

#include "qt_theme.hpp"

void
WindowsDarkModeFilter::reselectDarkMode()
{
    const bool oldDarkMode = theme::isDarkThemeActive();
    theme::applyAppTheme();
    window->resize(window->size());
    window->updateDarkMode();

    if (theme::isDarkThemeActive() != oldDarkMode)
        QTimer::singleShot(1000, [this]() {
            theme::syncNativeWindowFrame(window);
        });
}

void
WindowsDarkModeFilter::setWindow(VMManagerMainWindow *window)
{
    this->window = window;
}

bool
WindowsDarkModeFilter::nativeEventFilter(const QByteArray &eventType, void *message, result_t *result)
{
    if ((window != nullptr) && (eventType == "windows_generic_MSG")) {
        MSG *msg = static_cast<MSG *>(message);

        if (theme::useSystemTheme() && theme::isSystemThemeChangeMessage(msg))
            reselectDarkMode();
    }

    return false;
}
