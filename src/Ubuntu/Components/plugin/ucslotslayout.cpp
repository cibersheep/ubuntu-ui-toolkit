/*
 * Copyright 2015 Canonical Ltd.
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
 *
 * Author: Andrea Bernabei <andrea.bernabei@canonical.com>
 */

#include <QtQml/QQmlEngine>
#include <QtQml/QQmlInfo>

#include "ucunits.h"
#include "ucslotslayout.h"
#include "ucslotslayout_p.h"
#include "ucfontutils.h"
#include "uctheme.h"
#include "unitythemeiconprovider.h"

/******************************************************************************
 * UCSlotsLayoutPrivate
 */
UCSlotsLayoutPrivate::UCSlotsLayoutPrivate()
    : QQuickItemPrivate()
    , mainSlot(Q_NULLPTR)
    , m_parentItem(Q_NULLPTR)
    , maxSlotsHeight(0)
    , _q_cachedHeight(-1)
    , maxNumberOfLeadingSlots(1)
    , maxNumberOfTrailingSlots(2)
{
}

UCSlotsLayoutPrivate::~UCSlotsLayoutPrivate()
{
}

void UCSlotsLayoutPrivate::init()
{
    Q_Q(UCSlotsLayout);

    _q_updateGuValues();

    // FIXME: This is not yet possible from C++ since we're not a StyledItem
    //QObject::connect(q, SIGNAL(themeChanged()),
    //                 q, SLOT(_q_onThemeChanged()), Qt::DirectConnection);

    QObject::connect(&padding, SIGNAL(leadingChanged()), q, SLOT(_q_relayout()));
    QObject::connect(&padding, SIGNAL(trailingChanged()), q, SLOT(_q_relayout()));
    QObject::connect(&padding, SIGNAL(topChanged()), q, SLOT(_q_updateSlotsBBoxHeight()));
    QObject::connect(&padding, SIGNAL(bottomChanged()), q, SLOT(_q_updateSlotsBBoxHeight()));

    QObject::connect(&UCUnits::instance(), SIGNAL(gridUnitChanged()), q, SLOT(_q_onGuValueChanged()));

    //FIXME (if possible): this will cause relayout to be called 4-5 times when the layout has "anchors.fill: parent"
    //defined on QML side
    QObject::connect(q, SIGNAL(widthChanged()), q, SLOT(_q_relayout()));

    //we connect height changes to a different function, because height changes only cause a relayout
    //in some cases (for instance when height goes from 0 to non 0)
    QObject::connect(q, SIGNAL(heightChanged()), q, SLOT(_q_updateCachedHeight()));

    QObject::connect(q, SIGNAL(visibleChanged()), q, SLOT(_q_relayout()));
}

UCSlotsLayoutPrivate::UCSlotPositioningMode UCSlotsLayoutPrivate::getVerticalPositioningMode() const
{
    return (mainSlotHeight > maxSlotsHeight)
            ? UCSlotPositioningMode::AlignToTop
            : UCSlotPositioningMode::CenterVertically;
}

void UCSlotsLayoutPrivate::updateTopBottomPaddingIfNeeded()
{
    if (!padding.topWasSetFromQml) {
        padding.setTop((getVerticalPositioningMode() == UCSlotPositioningMode::CenterVertically
                                     && maxSlotsHeight > UCUnits::instance().gu(SLOTSLAYOUT_TOPBOTTOMMARGIN_SIZETHRESHOLD_GU))
                                    ? UCUnits::instance().gu(SLOTSLAYOUT_TOPMARGIN1_GU)
                                    : UCUnits::instance().gu(SLOTSLAYOUT_TOPMARGIN2_GU));
    }

    if (!padding.bottomWasSetFromQml) {
        padding.setBottom((getVerticalPositioningMode() == UCSlotPositioningMode::CenterVertically
                                        && maxSlotsHeight > UCUnits::instance().gu(SLOTSLAYOUT_TOPBOTTOMMARGIN_SIZETHRESHOLD_GU))
                                       ? UCUnits::instance().gu(SLOTSLAYOUT_BOTTOMMARGIN1_GU)
                                       : UCUnits::instance().gu(SLOTSLAYOUT_BOTTOMMARGIN2_GU));
    }

    return;
}

void UCSlotsLayoutPrivate::_q_onGuValueChanged()
{
    _q_onMainSlotHeightChanged();
    _q_updateSlotsBBoxHeight();
    _q_updateGuValues();
}

void UCSlotsLayoutPrivate::_q_updateCachedHeight()
{
    Q_Q(UCSlotsLayout);
    if (_q_cachedHeight != q->height()) {
        if (qIsNull(_q_cachedHeight)) {
            _q_relayout();
        }
        _q_cachedHeight = q->height();
    }
}

void UCSlotsLayoutPrivate::_q_updateGuValues()
{
    Q_Q(UCSlotsLayout);

    if (!padding.leadingWasSetFromQml) {
        padding.setLeading(UCUnits::instance().gu(SLOTSLAYOUT_LEFTMARGIN_GU));
    }

    if (!padding.trailingWasSetFromQml) {
        padding.setTrailing(UCUnits::instance().gu(SLOTSLAYOUT_RIGHTMARGIN_GU));
    }

    updateTopBottomPaddingIfNeeded();

    _q_updateSize();
}

void UCSlotsLayoutPrivate::_q_onMainSlotHeightChanged()
{
    //if the component is not ready the QML properties may not have been evaluated yet,
    //it's not worth doing anything if that's the case
    if (!componentComplete)
        return;

    Q_Q(UCSlotsLayout);

    if (mainSlot) {
        mainSlotHeight = mainSlot->height();
    } else {
        mainSlotHeight = 0;
    }

    updateTopBottomPaddingIfNeeded();
    _q_updateSize();
}

void UCSlotsLayoutPrivate::_q_updateSlotsBBoxHeight()
{
    if (!componentComplete)
        return;

    Q_Q(UCSlotsLayout);

    qreal maxSlotsHeightTmp = 0;
    const int size = q->childItems().count();
    for (int i = 0; i < size; i++) {
        QQuickItem *child = q->childItems().at(i);

        //skip mainSlot, we want to know the max height of the slots, mainSlot excluded
        if (child == mainSlot)
            continue;

        //ignore children which have custom vertical positioning
        UCSlotsAttached *attachedProperty =
                qobject_cast<UCSlotsAttached *>(qmlAttachedPropertiesObject<UCSlotsLayout>(child));

        if (!attachedProperty) {
            qmlInfo(q) << "Invalid attached property!";
            continue;
        }

        if (!attachedProperty->overrideVerticalPositioning())
            maxSlotsHeightTmp = qMax<qreal>(maxSlotsHeightTmp, child->height());
    }
    maxSlotsHeight = maxSlotsHeightTmp;

    updateTopBottomPaddingIfNeeded();
    _q_updateSize();
}

void UCSlotsLayoutPrivate::_q_updateSize()
{
    if (!componentComplete)
        return;

    Q_Q(UCSlotsLayout);
    q->setImplicitWidth(parentItem ? parentItem->width() : UCUnits::instance().gu(IMPLICIT_SLOTSLAYOUT_WIDTH_GU));
    q->setImplicitHeight(qMax<qreal>(maxSlotsHeight, mainSlotHeight)
                         + padding.top() + padding.bottom());

    _q_relayout();
}

qreal UCSlotsLayoutPrivate::populateSlotsListsAndComputeWidth()
{
    Q_Q(UCSlotsLayout);

    leadingSlots.clear();
    trailingSlots.clear();
    int numberOfLeadingBeginningSlots = 0;
    int numberOfLeadingEndSlots = 0;
    int numberOfTrailingBeginningSlots = 0;
    int numberOfTrailingEndSlots = 0;
    int indexToInsertAt = -1;

    //the total width of the visible slots, paddings included
    qint32 totalWidth = 0;

    const int size = q->childItems().count();
    for (int i = 0; i < size; i++) {
        QQuickItem *child = q->childItems().at(i);
        indexToInsertAt = -1;

        //NOTE: skip mainSlot, as we handle is separately
        if (child == mainSlot) {
            continue;
        }

        if (!child->isVisible()) {
            continue;
        }

        UCSlotsAttached *attached =
                qobject_cast<UCSlotsAttached *>(qmlAttachedPropertiesObject<UCSlotsLayout>(child));

        if (!attached) {
            qmlInfo(q) << "Invalid attached property!";
            continue;
        }

        if (attached->position() == UCSlotsLayout::Leading
                || attached->position() == UCSlotsLayout::LeadingBeginning
                || attached->position() == UCSlotsLayout::LeadingEnd) {

            int totalLeadingSlots = leadingSlots.length();
            if (leadingSlots.length() < maxNumberOfLeadingSlots) {
                switch (attached->position()) {
                case UCSlotsLayout::LeadingBeginning:
                    indexToInsertAt = numberOfLeadingBeginningSlots;
                    ++numberOfLeadingBeginningSlots;
                    break;
                case UCSlotsLayout::Leading:
                    indexToInsertAt = totalLeadingSlots - numberOfLeadingEndSlots;
                    break;
                case UCSlotsLayout::LeadingEnd:
                    indexToInsertAt = totalLeadingSlots;
                    ++numberOfLeadingEndSlots;
                    break;
                default:
                    break;
                }

                leadingSlots.insert(indexToInsertAt, child);
                totalWidth += child->width() + attached->leadingPadding() + attached->trailingPadding();
            } else {
                qmlInfo(q) << "This layout only allows up to " << maxNumberOfLeadingSlots
                           << " leading slots. Please remove any additional leading slot.";
                continue;
            }

        } else if (attached->position() == UCSlotsLayout::Trailing
                   || attached->position() == UCSlotsLayout::TrailingBeginning
                   || attached->position() == UCSlotsLayout::TrailingEnd) {

            int totalTrailingSlots = trailingSlots.length();
            if (trailingSlots.length() < maxNumberOfTrailingSlots) {
                switch (attached->position()) {
                case UCSlotsLayout::TrailingBeginning:
                    indexToInsertAt = numberOfTrailingBeginningSlots;
                    ++numberOfTrailingBeginningSlots;
                    break;
                case UCSlotsLayout::Trailing:
                    indexToInsertAt = totalTrailingSlots - numberOfTrailingEndSlots;
                    break;
                case UCSlotsLayout::TrailingEnd:
                    indexToInsertAt = totalTrailingSlots;
                    ++numberOfTrailingEndSlots;
                    break;
                default:
                    break;
                }

                trailingSlots.insert(indexToInsertAt, child);
                totalWidth += child->width() + attached->leadingPadding() + attached->trailingPadding();
            } else {
                qmlInfo(q) << "This layout only allows up to " << maxNumberOfTrailingSlots
                           << " trailing slots. Please remove any additional trailing slot.";
                continue;
            }
        } else {
            qmlInfo(q) << "Unrecognized position value!";
            continue;
        }
    }

    return totalWidth;
}

void UCSlotsLayoutPrivate::setupSlotsVerticalPositioning(QQuickItem *slot)
{
    if (slot == Q_NULLPTR)
        return;

    QQuickAnchors *slotAnchors = QQuickItemPrivate::get(slot)->anchors();

    if (getVerticalPositioningMode() == UCSlotPositioningMode::AlignToTop) {
        //reset the vertical anchor as we might be transitioning from the configuration
        //where all items are vertically centered to the one where they're anchored to top
        slotAnchors->resetVerticalCenter();
        slotAnchors->setVerticalCenterOffset(0);

        slotAnchors->setTop(top());
        slotAnchors->setTopMargin(padding.top());
    } else {
        slotAnchors->resetTop();

        slotAnchors->setVerticalCenter(verticalCenter());
        //bottom and top offsets could have different values
        slotAnchors->setVerticalCenterOffset((padding.top() - padding.bottom()) / 2.0);
    }
}

void UCSlotsLayoutPrivate::layoutInRow(qreal siblingAnchorMargin, QQuickAnchorLine siblingAnchor, QList<QQuickItem *> &items)
{
    Q_Q(UCSlotsLayout);

    const int size = items.length();
    for (int i = 0; i < size; i++) {
        QQuickItem *item = items.at(i);
        QQuickAnchors *itemAnchors = QQuickItemPrivate::get(item)->anchors();

        UCSlotsAttached *attached =
                qobject_cast<UCSlotsAttached *>(qmlAttachedPropertiesObject<UCSlotsLayout>(item));

        if (!attached) {
            qmlInfo(q) << "Invalid attached property!";
            continue;
        }

        if (!attached->overrideVerticalPositioning()) {
            setupSlotsVerticalPositioning(item);
        }

        if (i == 0) {
            //skip anchoring if the anchor given as input is invalid
            if (siblingAnchor.item == Q_NULLPTR)
                continue;

            itemAnchors->setLeft(siblingAnchor);
            itemAnchors->setLeftMargin(attached->leadingPadding() + siblingAnchorMargin);
        } else {
            UCSlotsAttached *attachedPreviousItem =
                    qobject_cast<UCSlotsAttached *>(qmlAttachedPropertiesObject<UCSlotsLayout>(items.at(i - 1)));

            if (!attachedPreviousItem) {
                qmlInfo(q) << "Invalid attached property!";
            } else {
                itemAnchors->setLeft(QQuickItemPrivate::get(items.at(i - 1))->right());
                itemAnchors->setLeftMargin(attachedPreviousItem->trailingPadding() + attached->leadingPadding());
            }
        }
    }
}

void UCSlotsLayoutPrivate::_q_relayout()
{
    Q_Q(UCSlotsLayout);

    //only relayout after the component has been initialized
    if (!componentComplete)
        return;

    if (q->width() <= 0 || q->height() <= 0
            || !q->isVisible() || !q->opacity()) {
        return;
    }

    qreal totalSlotsWidth = populateSlotsListsAndComputeWidth();

    QList<QQuickItem *> itemsToLayout(leadingSlots);

    if (mainSlot) {
        itemsToLayout.append(mainSlot);

        UCSlotsAttached *attachedSlot =
                qobject_cast<UCSlotsAttached *>(qmlAttachedPropertiesObject<UCSlotsLayout>(mainSlot));

        if (!attachedSlot) {
            qmlInfo(q) << "Invalid attached property!";
            return;
        }
        mainSlot->setImplicitWidth(q->width() - totalSlotsWidth
                           - attachedSlot->leadingPadding() - attachedSlot->trailingPadding()
                           - padding.leading() - padding.trailing());
    } else {
        Q_UNUSED(totalSlotsWidth);
    }

    itemsToLayout.append(trailingSlots);

    layoutInRow(padding.leading(), left(), itemsToLayout);
}

void UCSlotsLayoutPrivate::handleAttachedPropertySignals(QQuickItem *item, bool connect)
{
    Q_Q(UCSlotsLayout);
    UCSlotsAttached *attachedSlot =
            qobject_cast<UCSlotsAttached *>(qmlAttachedPropertiesObject<UCSlotsLayout>(item));

    if (!attachedSlot) {
        qmlInfo(q) << "Invalid attached property!";
        return;
    }

    if (connect) {
        QObject::connect(attachedSlot, SIGNAL(positionChanged()), q, SLOT(_q_relayout()));
        QObject::connect(attachedSlot, SIGNAL(leadingPaddingChanged()), q, SLOT(_q_relayout()));
        QObject::connect(attachedSlot, SIGNAL(trailingPaddingChanged()), q, SLOT(_q_relayout()));
        QObject::connect(attachedSlot, SIGNAL(overrideVerticalPositioningChanged()), q, SLOT(_q_relayout()));
    } else {
        QObject::disconnect(attachedSlot, SIGNAL(positionChanged()), q, SLOT(_q_relayout()));
        QObject::disconnect(attachedSlot, SIGNAL(leadingPaddingChanged()), q, SLOT(_q_relayout()));
        QObject::disconnect(attachedSlot, SIGNAL(trailingPaddingChanged()), q, SLOT(_q_relayout()));
        QObject::disconnect(attachedSlot, SIGNAL(overrideVerticalPositioningChanged()), q, SLOT(_q_relayout()));
    }
}

UCSlotsLayout::UCSlotsLayout(QQuickItem *parent) :
    QQuickItem(*(new UCSlotsLayoutPrivate), parent)
{
    setFlag(ItemHasContents);
    setFlag(ItemIsFocusScope);
    Q_D(UCSlotsLayout);
    d->init();
}

void UCSlotsLayout::componentComplete()
{
    Q_D(UCSlotsLayout);
    QQuickItem::componentComplete();

    d->_q_onMainSlotHeightChanged();
    d->_q_updateSlotsBBoxHeight();
}

void UCSlotsLayout::itemChange(ItemChange change, const ItemChangeData &data)
{
    Q_D(UCSlotsLayout);

    //declare vars outside switch to prevent "crosses initialization of" compile error
    QQuickItem *newParent = Q_NULLPTR;
    switch (change) {
    case ItemChildAddedChange:
        if (data.item) {
            d->handleAttachedPropertySignals(data.item, true);

            QObject::connect(data.item, SIGNAL(visibleChanged()), this, SLOT(_q_relayout()));

            //we relayout because we have to update the width of the main slot
            //TODO: do this in a separate function? do were really have to do the whole relayout?
            if (data.item != d->mainSlot) {
                QObject::connect(data.item, SIGNAL(widthChanged()), this, SLOT(_q_relayout()));
                QObject::connect(data.item, SIGNAL(heightChanged()), this, SLOT(_q_updateSlotsBBoxHeight()));
                d->_q_updateSlotsBBoxHeight();
            } else {
                QObject::connect(data.item, SIGNAL(heightChanged()), this, SLOT(_q_onMainSlotHeightChanged()));
                d->_q_onMainSlotHeightChanged();
            }
        }
        break;
    case ItemChildRemovedChange:
        if (data.item) {
            d->handleAttachedPropertySignals(data.item, false);

            //This wouldn't be needed if the child is destroyed, but we can't know what, we just know
            //that it's changing parent, so we still disconnect from all the signals manually
            QObject::disconnect(data.item, SIGNAL(visibleChanged()), this, SLOT(_q_relayout()));

            if (data.item != d->mainSlot) {
                QObject::disconnect(data.item, SIGNAL(widthChanged()), this, SLOT(_q_relayout()));
                QObject::disconnect(data.item, SIGNAL(heightChanged()), this, SLOT(_q_updateSlotsBBoxHeight()));
                d->_q_updateSlotsBBoxHeight();
            } else {
                QObject::disconnect(data.item, SIGNAL(heightChanged()), this, SLOT(_q_onMainSlotHeightChanged()));
                d->_q_onMainSlotHeightChanged();
            }
        }

        break;
    case ItemParentHasChanged:
        newParent = data.item;
        if (newParent) {
            if (d->m_parentItem) {
                QObject::disconnect(d->m_parentItem, SIGNAL(widthChanged()), this, SLOT(_q_updateSize()));
            }

            d->m_parentItem = newParent;
            QObject::connect(newParent, SIGNAL(widthChanged()), this, SLOT(_q_updateSize()), Qt::DirectConnection);
            d->_q_updateSize();
        }
        break;
    default:
        break;
    }

    QQuickItem::itemChange(change, data);
}

QQuickItem *UCSlotsLayout::mainSlot() const
{
    Q_D(const UCSlotsLayout);
    return d->mainSlot;
}

void UCSlotsLayout::setMainSlot(QQuickItem *item)
{
    Q_D(UCSlotsLayout);
    if (d->mainSlot != item) {
        d->mainSlot = item;
        d->mainSlot->setParent(this);
        d->mainSlot->setParentItem(this);
        Q_EMIT mainSlotChanged();
    }
}

UCSlotsLayoutPadding* UCSlotsLayout::padding() {
    Q_D(UCSlotsLayout);
    return &(d->padding);
}

/******************************************************************************
 * UCSlotsAttachedPrivate
 */
UCSlotsAttachedPrivate::UCSlotsAttachedPrivate()
    : QObjectPrivate()
    , position(UCSlotsLayout::Trailing)
    , leadingPadding(0)
    , trailingPadding(0)
    , overrideVerticalPositioning(false)
    , leadingPaddingWasSetFromQml(false)
    , trailingPaddingWasSetFromQml(false)
{
}

void UCSlotsAttachedPrivate::_q_onGuValueChanged()
{
    Q_Q(UCSlotsAttached);
    if (!leadingPaddingWasSetFromQml)
        q->setLeadingPadding(UCUnits::instance().gu(SLOTSLAYOUT_SLOTS_SIDEMARGINS_GU));

    if (!trailingPaddingWasSetFromQml)
        q->setTrailingPadding(UCUnits::instance().gu(SLOTSLAYOUT_SLOTS_SIDEMARGINS_GU));
}

/******************************************************************************
 * UCSlotsAttached
 */
UCSlotsAttached::UCSlotsAttached(QObject *object)
    : QObject(*(new UCSlotsAttachedPrivate), object)
{
    Q_D(UCSlotsAttached);
    d->_q_onGuValueChanged();
    QObject::connect(&UCUnits::instance(), SIGNAL(gridUnitChanged()), this, SLOT(_q_onGuValueChanged()));
}

UCSlotsLayout::UCSlotPosition UCSlotsAttached::position() const
{
    Q_D(const UCSlotsAttached);
    return d->position;
}

void UCSlotsAttached::setPosition(UCSlotsLayout::UCSlotPosition pos)
{
    Q_D(UCSlotsAttached);
    if (d->position != pos) {
        d->position = pos;
        Q_EMIT positionChanged();
    }
}

qreal UCSlotsAttached::leadingPadding() const
{
    Q_D(const UCSlotsAttached);
    return d->leadingPadding;
}

void UCSlotsAttached::setLeadingPadding(qreal padding)
{
    Q_D(UCSlotsAttached);
    if (d->leadingPadding != padding) {
        d->leadingPadding = padding;
        Q_EMIT leadingPaddingChanged();
    }
}

void UCSlotsAttached::setLeadingPaddingQml(qreal padding)
{
    Q_D(UCSlotsAttached);
    d->leadingPaddingWasSetFromQml = true;

    //if both have been set from QML, then disconnect the signal from the slot, to avoid overwriting dev's values
    //when GU changes
    if (d->trailingPaddingWasSetFromQml)
        QObject::disconnect(&UCUnits::instance(), SIGNAL(gridUnitChanged()), this, SLOT(_q_onGuValueChanged()));

    setLeadingPadding(padding);
}

qreal UCSlotsAttached::trailingPadding() const
{
    Q_D(const UCSlotsAttached);
    return d->trailingPadding;
}

void UCSlotsAttached::setTrailingPadding(qreal padding)
{
    Q_D(UCSlotsAttached);
    if (d->trailingPadding != padding) {
        d->trailingPadding = padding;
        Q_EMIT trailingPaddingChanged();
    }
}

void UCSlotsAttached::setTrailingPaddingQml(qreal padding)
{
    Q_D(UCSlotsAttached);
    d->trailingPaddingWasSetFromQml = true;

    //if both have been set from QML, then disconnect the signal from the slot, to avoid overwriting dev's values
    //when GU changes
    if (d->leadingPaddingWasSetFromQml)
        QObject::disconnect(&UCUnits::instance(), SIGNAL(gridUnitChanged()), this, SLOT(_q_onGuValueChanged()));

    setTrailingPadding(padding);
}

bool UCSlotsAttached::overrideVerticalPositioning() const
{
    Q_D(const UCSlotsAttached);
    return d->overrideVerticalPositioning;
}

void UCSlotsAttached::setOverrideVerticalPositioning(bool val)
{
    Q_D(UCSlotsAttached);
    if (d->overrideVerticalPositioning != val) {
        d->overrideVerticalPositioning = val;
        Q_EMIT overrideVerticalPositioningChanged();
    }
}

UCSlotsAttached *UCSlotsLayout::qmlAttachedProperties(QObject *object)
{
    return new UCSlotsAttached(object);
}

UCSlotsLayoutPadding::UCSlotsLayoutPadding(QObject *parent)
    : QObject(parent)
    , leadingWasSetFromQml(false)
    , trailingWasSetFromQml(false)
    , topWasSetFromQml(false)
    , bottomWasSetFromQml(false)
    , m_leading(0)
    , m_trailing(0)
    , m_top(0)
    , m_bottom(0)
{
}

qreal UCSlotsLayoutPadding::leading() const
{
    return m_leading;
}

void UCSlotsLayoutPadding::setLeading(qreal val)
{
    if (m_leading != val) {
        m_leading = val;
        Q_EMIT leadingChanged();
    }
}

void UCSlotsLayoutPadding::setLeadingQml(qreal val)
{
    leadingWasSetFromQml = true;
    setLeading(val);
}

qreal UCSlotsLayoutPadding::trailing() const
{
    return m_trailing;
}

void UCSlotsLayoutPadding::setTrailing(qreal val)
{
    if (m_trailing != val) {
        m_trailing = val;
        Q_EMIT trailingChanged();
    }
}

void UCSlotsLayoutPadding::setTrailingQml(qreal val)
{
    trailingWasSetFromQml = true;
    setTrailing(val);
}

qreal UCSlotsLayoutPadding::top() const
{
    return m_top;
}

void UCSlotsLayoutPadding::setTop(qreal val)
{
    if (m_top != val) {
        m_top = val;
        Q_EMIT topChanged();
    }
}

void UCSlotsLayoutPadding::setTopQml(qreal val)
{
    topWasSetFromQml = true;
    setTop(val);
}

qreal UCSlotsLayoutPadding::bottom() const
{
    return m_bottom;
}

void UCSlotsLayoutPadding::setBottom(qreal val)
{
    if (m_bottom != val) {
        m_bottom = val;
        Q_EMIT bottomChanged();
    }
}

void UCSlotsLayoutPadding::setBottomQml(qreal val)
{
    bottomWasSetFromQml = true;
    setBottom(val);
}


UCThreeLabelsSlotPrivate::UCThreeLabelsSlotPrivate()
    : QQuickItemPrivate()
    , cachedTheme(Q_NULLPTR)
    , m_title(Q_NULLPTR)
    , m_subtitle(Q_NULLPTR)
    , m_summary(Q_NULLPTR)
{
}

void UCThreeLabelsSlotPrivate::init()
{
    Q_Q(UCThreeLabelsSlot);

    //TODO FIXME: Add connect to themeChanged() when that signal will be available

    QObject::connect(&UCUnits::instance(), SIGNAL(gridUnitChanged()), q, SLOT(_q_onGuValueChanged()));
}

void UCThreeLabelsSlotPrivate::setTitleProperties()
{
    Q_Q(UCThreeLabelsSlot);
    if (m_title != Q_NULLPTR) {
        m_title->setWrapMode(QQuickText::WordWrap);
        m_title->setElideMode(QQuickText::ElideRight);
        m_title->setMaximumLineCount(1);
        QFont titleFont = m_title->font();
        titleFont.setPixelSize(UCFontUtils::instance().sizeToPixels("medium"));
        titleFont.setWeight(QFont::Light);
        m_title->setFont(titleFont);
    }

    //We set the theme-dependent properties (such as the colour) later
    //as it requires qmlContext(q) to be initialized
}

void UCThreeLabelsSlotPrivate::setSubtitleProperties()
{
    Q_Q(UCThreeLabelsSlot);
    if (m_subtitle != Q_NULLPTR) {
        m_subtitle->setWrapMode(QQuickText::WordWrap);
        m_subtitle->setElideMode(QQuickText::ElideRight);
        m_subtitle->setMaximumLineCount(1);
        QFont subtitleFont = m_subtitle->font();
        subtitleFont.setPixelSize(UCFontUtils::instance().sizeToPixels("small"));
        subtitleFont.setWeight(QFont::Light);
        m_subtitle->setFont(subtitleFont);
    }

    //We set the theme-dependent properties (such as the colour) later
    //as it requires qmlContext(q) to be initialized
}

void UCThreeLabelsSlotPrivate::updateLabelsColors()
{
    if (!componentComplete)
        return;

    if (cachedTheme) {
        if (m_title != Q_NULLPTR) {
            m_title->setColor(labelsColor);
        }
        if (m_subtitle != Q_NULLPTR) {
            m_subtitle->setColor(labelsColor);
        }
        if (m_summary != Q_NULLPTR) {
            m_summary->setColor(labelsColor);
        }
    }
}

void UCThreeLabelsSlotPrivate::setSummaryProperties()
{
    Q_Q(UCThreeLabelsSlot);
    if (m_summary != Q_NULLPTR) {
        m_summary->setWrapMode(QQuickText::WordWrap);
        m_summary->setElideMode(QQuickText::ElideRight);
        m_summary->setMaximumLineCount(2);
        QFont summaryFont = m_summary->font();
        summaryFont.setPixelSize(UCFontUtils::instance().sizeToPixels("small"));
        summaryFont.setWeight(QFont::Light);
        m_summary->setFont(summaryFont);
    }

    //We set the theme-dependent properties (such as the colour) later
    //as it requires qmlContext(q) to be initialized
}

void UCThreeLabelsSlotPrivate::_q_onThemeChanged()
{
    if (!componentComplete)
        return;

    Q_Q(UCThreeLabelsSlot);
    cachedTheme = qmlContext(q)->contextProperty("theme").value<UCTheme*>();
    labelsColor = cachedTheme->getPaletteColor("selected", "backgroundText");

    updateLabelsColors();
}

void UCThreeLabelsSlotPrivate::_q_onGuValueChanged()
{
    if (m_title != Q_NULLPTR) {
        QFont titleFont = m_title->font();
        titleFont.setPixelSize(UCFontUtils::instance().sizeToPixels("medium"));
        m_title->setFont(titleFont);
    }

    if (m_subtitle != Q_NULLPTR) {
        QFont subtitleFont = m_subtitle->font();
        subtitleFont.setPixelSize(UCFontUtils::instance().sizeToPixels("small"));
        m_subtitle->setFont(subtitleFont);
    }

    if (m_summary != Q_NULLPTR) {
        QFont summaryFont = m_summary->font();
        summaryFont.setPixelSize(UCFontUtils::instance().sizeToPixels("small"));
        m_summary->setFont(summaryFont);
    }

    if (m_title != Q_NULLPTR
            || m_subtitle != Q_NULLPTR
            || m_summary != Q_NULLPTR) {
        _q_updateLabelsAnchorsAndBBoxHeight();
    }
}

void UCThreeLabelsSlotPrivate::_q_updateLabelsAnchorsAndBBoxHeight()
{
    //if the component is not ready the QML properties may not have been evaluated yet,
    //it's not worth doing anything if that's the case
    if (!componentComplete) {
        return;
    }

    Q_Q(UCThreeLabelsSlot);
    bool skipTitle = m_title == Q_NULLPTR || m_title->text().isEmpty() || !m_title->isVisible();
    bool skipSubtitle = m_subtitle == Q_NULLPTR || m_subtitle->text().isEmpty() || !m_subtitle->isVisible();
    bool skipSummary = m_summary == Q_NULLPTR || m_summary->text().isEmpty() || !m_summary->isVisible();

    if (!skipTitle) {
        QQuickAnchors *titleAnchors = QQuickItemPrivate::get(m_title)->anchors();
        titleAnchors->setTop(top());
    }

    //even if a QQuickText is empty it will have height as if it had one character, so we can't rely
    //on just anchoring to bottom of the text above us (title in this case) because that will lead
    //to wrong positioning when title is empty
    if (!skipSubtitle) {
        QQuickAnchors *subtitleAnchors = QQuickItemPrivate::get(m_subtitle)->anchors();
        subtitleAnchors->setTop(skipTitle
                                ? top()
                                : QQuickItemPrivate::get(m_title)->baseline());
        subtitleAnchors->setTopMargin(skipTitle
                                      ? 0
                                      : UCUnits::instance().gu(LABELSBLOCK_SPACING_GU));
    }

    if (!skipSummary) {
        QQuickAnchors *summaryAnchors = QQuickItemPrivate::get(m_summary)->anchors();
        summaryAnchors->setTop(skipSubtitle
                               ? (skipTitle ? top() : QQuickItemPrivate::get(m_title)->baseline())
                               : QQuickItemPrivate::get(m_subtitle)->baseline());
        summaryAnchors->setTopMargin(skipSubtitle && skipTitle
                                     ? 0
                                     : UCUnits::instance().gu(LABELSBLOCK_SPACING_GU));

    }
    //Update height of the labels box
    //NOTE (FIXME? it's stuff in Qt): height is not 0 when the string is empty, its default value is "fontHeight"!
    qreal labelsBoundingBoxHeight = 0;

    if (!skipTitle) {
        if (skipSubtitle && skipSummary) {
            labelsBoundingBoxHeight += m_title->height();
        } else {
            labelsBoundingBoxHeight += m_title->baselineOffset() + UCUnits::instance().gu(LABELSBLOCK_SPACING_GU);
        }
    }

    if (skipSubtitle) {
        if (!skipSummary) {
            labelsBoundingBoxHeight += m_summary->height();
        }
    } else {
        if (skipSummary) {
            labelsBoundingBoxHeight += m_subtitle->height();
        } else {
            labelsBoundingBoxHeight += m_subtitle->baselineOffset()
                    + UCUnits::instance().gu(LABELSBLOCK_SPACING_GU)
                    + m_summary->height();
        }
    }

    q->setImplicitHeight(labelsBoundingBoxHeight);
}

UCThreeLabelsSlot::UCThreeLabelsSlot(QQuickItem *parent)
    : QQuickItem(*(new UCThreeLabelsSlotPrivate), parent)
{
    setFlag(ItemHasContents);
    Q_D(UCThreeLabelsSlot);
    d->init();
}

void UCThreeLabelsSlot::componentComplete()
{
    Q_D(UCThreeLabelsSlot);
    QQuickItem::componentComplete();

    d->_q_onGuValueChanged();

    //only at this point the context property is available
    d->_q_onThemeChanged();
}

QQuickText *UCThreeLabelsSlot::title()
{
    Q_D(UCThreeLabelsSlot);
    if (d->m_title == Q_NULLPTR) {
        d->m_title = new QQuickText(this);

        QQuickAnchors *titleAnchors = QQuickItemPrivate::get(d->m_title)->anchors();
        titleAnchors->setLeft(d->left());
        titleAnchors->setRight(d->right());

        //we need this to know when any of the labels is empty. In that case, we'll have to change the
        //anchors because even if a QQuickText has empty text, its height will not be 0 but "fontHeight",
        //so anchoring to text's bottom will result in the wrong outcome as a consequence.
        //TODO: updating anchors just because text changes is too much, we should probably just
        //have variables signal when a label becomes empty
        QObject::connect(d->m_title, SIGNAL(textChanged(QString)), this, SLOT(_q_updateLabelsAnchorsAndBBoxHeight()));

        //the height may change for many reasons, for instance:
        //- change of fontsize
        //- or resizing the layout until text wrapping is triggered
        //so we have to monitor height change as well
        QObject::connect(d->m_title, SIGNAL(heightChanged()), this, SLOT(_q_updateLabelsAnchorsAndBBoxHeight()));
        QObject::connect(d->m_title, SIGNAL(visibleChanged()), this, SLOT(_q_updateLabelsAnchorsAndBBoxHeight()));

        d->setTitleProperties();
        d->updateLabelsColors();
        d->_q_updateLabelsAnchorsAndBBoxHeight();
    }
    return d->m_title;
}

QQuickText *UCThreeLabelsSlot::subtitle()
{
    Q_D(UCThreeLabelsSlot);
    if (d->m_subtitle == Q_NULLPTR) {
        d->m_subtitle = new QQuickText(this);

        QQuickAnchors *subtitleAnchors = QQuickItemPrivate::get(d->m_subtitle)->anchors();
        subtitleAnchors->setLeft(d->left());
        subtitleAnchors->setRight(d->right());

        QObject::connect(d->m_subtitle, SIGNAL(textChanged(QString)), this, SLOT(_q_updateLabelsAnchorsAndBBoxHeight()));
        QObject::connect(d->m_subtitle, SIGNAL(heightChanged()), this, SLOT(_q_updateLabelsAnchorsAndBBoxHeight()));
        QObject::connect(d->m_subtitle, SIGNAL(visibleChanged()), this, SLOT(_q_updateLabelsAnchorsAndBBoxHeight()));

        d->setSubtitleProperties();
        d->updateLabelsColors();
        d->_q_updateLabelsAnchorsAndBBoxHeight();
    }
    return d->m_subtitle;
}

QQuickText *UCThreeLabelsSlot::summary()
{
    Q_D(UCThreeLabelsSlot);
    if (d->m_summary == Q_NULLPTR) {
        d->m_summary = new QQuickText(this);

        QQuickAnchors *summaryAnchors = QQuickItemPrivate::get(d->m_summary)->anchors();
        summaryAnchors->setLeft(d->left());
        summaryAnchors->setRight(d->right());

        QObject::connect(d->m_summary, SIGNAL(textChanged(QString)), this, SLOT(_q_updateLabelsAnchorsAndBBoxHeight()));
        QObject::connect(d->m_summary, SIGNAL(heightChanged()), this, SLOT(_q_updateLabelsAnchorsAndBBoxHeight()));
        QObject::connect(d->m_summary, SIGNAL(visibleChanged()), this, SLOT(_q_updateLabelsAnchorsAndBBoxHeight()));

        d->setSummaryProperties();
        d->updateLabelsColors();
        d->_q_updateLabelsAnchorsAndBBoxHeight();
    }
    return d->m_summary;
}

#include "moc_ucslotslayout.cpp"
