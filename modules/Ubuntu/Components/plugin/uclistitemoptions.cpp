/*
 * Copyright 2014 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "uclistitemoptions.h"
#include "uclistitemoptions_p.h"
#include "uclistitem_p.h"
#include "quickutils.h"
#include "i18n.h"
#include <QtQml/QQmlInfo>

UCListItemOptionsPrivate::UCListItemOptionsPrivate()
    : QObjectPrivate()
    , delegate(0)
    , panel(0)
    , backgroundColor(Qt::red)
{
}
UCListItemOptionsPrivate::~UCListItemOptionsPrivate()
{
}

void UCListItemOptionsPrivate::funcAppend(QQmlListProperty<QObject> *list, QObject *option)
{
    UCListItemOptions *_this = static_cast<UCListItemOptions*>(list->object);
    UCListItemOptionsPrivate *plist = UCListItemOptionsPrivate::get(_this);
    if (!QuickUtils::inherits(option, "Action")) {
        qmlInfo(_this) << UbuntuI18n::instance().tr(QString("Option at index %1 is not an Action or a derivate of it.").arg(plist->options.size()));
        return;
    }
    plist->options.append(option);
}
int UCListItemOptionsPrivate::funcCount(QQmlListProperty<QObject> *list)
{
    UCListItemOptions *_this = static_cast<UCListItemOptions*>(list->object);
    UCListItemOptionsPrivate *plist = UCListItemOptionsPrivate::get(_this);
    return plist->options.size();
}
QObject *UCListItemOptionsPrivate::funcAt(QQmlListProperty<QObject> *list, int index)
{
    UCListItemOptions *_this = static_cast<UCListItemOptions*>(list->object);
    UCListItemOptionsPrivate *plist = UCListItemOptionsPrivate::get(_this);
    return plist->options.at(index);
}
void UCListItemOptionsPrivate::funcClear(QQmlListProperty<QObject> *list)
{
    UCListItemOptions *_this = static_cast<UCListItemOptions*>(list->object);
    UCListItemOptionsPrivate *plist = UCListItemOptionsPrivate::get(_this);
    return plist->options.clear();
}

/*!
 * \qmltype ListItemOptions
 * \instantiates UCListItemOptions
 * \inherits QtQObject
 * \inqmlmodule Ubuntu.Components 1.1
 * \ingroup ubuntu
 * \brief Provides configuration for options to be added to a ViewItem.
 *
 * ViewItem accepts options that can be configured to appear when swiped to left
 * or right. There is no limitation on how many options can be displayed on both
 * sides, this depends on the space available. However design guides say that it
 * can be a maximum of one option on the left (leadng) and a maximum of 3 options
 * on the right (trailing) side of a ViewItem.
 *
 * The options are Action instances or elements derived from Action. Visualization
 * of the options can be configured using the \l delegate property, and the default
 * implementation uses the \c name property of the Action.
 *
 * The leading and trailing options are placed on a panel which has different colors
 * for each side. This can be configured using the \l backgroundColor property.
 *
 * When tugged, panels reveal the options one by one. In case an option is revealed
 * more than 50%, the option will be snapped and revealed completely. Options can be
 * triggered by tapping.
 *
 * \section3 Notes on performance
 * When used with views, or when the amount of items of same kind to be created
 * is huge, it is recommended to use cached actions as well as cached ViewItemOption
 * instances. In this way we can reduce the creation time of the items:
 * \qml
 * import QtQuick 2.2
 * import Ubuntu.Components 1.1
 *
 * MainView {
 *     width: units.gu(40)
 *     height: units.gu(71)
 *
 *     ListItemOptions {
 *         id: commonOptions
 *         Action {
 *             iconName: "search"
 *         }
 *         Action {
 *             iconName: "edit"
 *         }
 *         Action {
 *             iconName: "copy"
 *         }
 *     }
 *     UbuntuListView {
 *         anchors.fill: parent
 *         model: 10000
 *         delegate: ListItem {
 *             trailingOptions: commonOptions
 *         }
 *     }
 * }
 * \endqml
 */

UCListItemOptions::UCListItemOptions(QObject *parent)
    : QObject(*(new UCListItemOptionsPrivate), parent)
{
}
UCListItemOptions::~UCListItemOptions()
{
}

/*!
 * \qmlproperty Component ListItemOptions::delegate
 * Custom delegate which overrides the default one used by the ViewItem. The default
 * value is null.
 */
QQmlComponent *UCListItemOptions::delegate() const
{
    Q_D(const UCListItemOptions);
    return d->delegate;
}
void UCListItemOptions::setDelegate(QQmlComponent *delegate)
{
    Q_D(UCListItemOptions);
    if (d->delegate == delegate) {
        return;
    }
    d->delegate = delegate;
    Q_EMIT delegateChanged();
}

/*!
 * \qmlproperty list<Action> ListItemOptions::options
 * \default
 * The property holds the options to be displayed. It can hold instances cached or
 * declared in place. An example of cached options:
 * \qml
 * ListItemOptions {
 *     id: cacedOptions
 *     options: [
 *         Action {
 *             iconName: "edit"
 *         },
 *         copyAction, searchAction, cutAction
 *     ]
 * }
 * \endqml
 */
QQmlListProperty<QObject> UCListItemOptions::options()
{
    Q_D(UCListItemOptions);
    return QQmlListProperty<QObject>(this, &(d->options),
                                     &UCListItemOptionsPrivate::funcAppend,
                                     &UCListItemOptionsPrivate::funcCount,
                                     &UCListItemOptionsPrivate::funcAt,
                                     &UCListItemOptionsPrivate::funcClear);
}

/*!
 * \qmlproperty color ListItemOptions::backgroundColor
 * The color used to override the panel background color holding the visualized
 * options.
 */
QColor UCListItemOptions::backgroundColor() const
{
    Q_D(const UCListItemOptions);
    return d->backgroundColor;
}
void UCListItemOptions::setBackgroundColor(const QColor &color)
{
    Q_D(UCListItemOptions);
    if (d->backgroundColor == color) {
        return;
    };
    d->backgroundColor = color;
    Q_EMIT backgroundColorChanged();
}
