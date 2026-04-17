#include "qt_perfdash_theme.hpp"

namespace perfdash::theme {

namespace {

const QColor kSurface("#17171C");
const QColor kSurfaceElevated("#1E1E26");
const QColor kBorder("#2A2A33");
const QColor kTextPrimary("#F2F2F5");
const QColor kTextSecondary("#A8A8B3");
const QColor kAccentBlue("#4F8CFF");
const QColor kAccentGreen("#3DD68C");
const QColor kAccentYellow("#F5C85B");
const QColor kAccentOrange("#FF9F55");
const QColor kAccentRed("#FF5E6C");
const QColor kAccentPurple("#B57BFF");

}

const QColor &
surface()
{
    return kSurface;
}

const QColor &
surfaceElevated()
{
    return kSurfaceElevated;
}

const QColor &
border()
{
    return kBorder;
}

const QColor &
textPrimary()
{
    return kTextPrimary;
}

const QColor &
textSecondary()
{
    return kTextSecondary;
}

const QColor &
accentBlue()
{
    return kAccentBlue;
}

const QColor &
accentGreen()
{
    return kAccentGreen;
}

const QColor &
accentYellow()
{
    return kAccentYellow;
}

const QColor &
accentOrange()
{
    return kAccentOrange;
}

const QColor &
accentRed()
{
    return kAccentRed;
}

const QColor &
accentPurple()
{
    return kAccentPurple;
}

}
