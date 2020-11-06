/*******************************************************************************
** Qt Advanced Docking System
** Copyright (C) 2017 Uwe Kindler
** 
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License as published by the Free Software Foundation; either
** version 2.1 of the License, or (at your option) any later version.
** 
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Lesser General Public License for more details.
** 
** You should have received a copy of the GNU Lesser General Public
** License along with this library; If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/


//============================================================================
/// \file   DockSplitter.cpp
/// \author Uwe Kindler
/// \date   24.03.2017
/// \brief  Implementation of CDockSplitter
//============================================================================

//============================================================================
//                                   INCLUDES
//============================================================================
#include "DockSplitter.h"

#include <cmath>

#include <QDebug>
#include <QChildEvent>
#include <QPainter>
#include <QStyleOption>
#include <QMouseEvent>

#include "DockAreaWidget.h"

#define RE_LOG_ENABLE
//#define RE_LOG_DEBUG_ENABLE
#include "NMLogger.h"

namespace ads
{

Splitter::Splitter(Qt::Orientation orient, QWidget *parent)
    : QWidget(parent)
{
    setOrientation(orient);
}

void Splitter::insertWidget(QWidget *widget, int index, int offset)
{
    if (widget == nullptr) {
        RE_LOG_ERROR("Widget is null");
        return;
    }
    if (widget->findChildren<Splitter *>().contains(this)) {
        RE_LOG_ERROR("Can't insert parent widget");
        return;
    }
    if (!indexInRange(index)) {
        index = count() - 1;
        offset = 1;
    }else if (offset < 0 || offset > 1) {
        offset = 1;
    }
    mBlockChildAdd = true;
    int wasIndex = indexOf(widget);
    if (wasIndex != -1) {
        std::swap(mWidgets[index], mWidgets[wasIndex]);
        std::swap(mParts[index], mParts[wasIndex]);
    } else {
        bool shouldRescaleVisible = false;
        if (equalParts()) {
            mParts.insert(index + offset, getEqualPart());
            rescaleParts();
        } else {
            int visibleIndex = findNextVisibleIndex(index, offset);
            mParts[visibleIndex] /= 2;
            mParts.insert(index + offset, mParts[visibleIndex]);
            shouldRescaleVisible = true;
        }
        mWidgets.insert(index + offset, widget);
        widget->setParent(this);
        if (shouldShow(widget)) {
            widget->show();
        }
        if (shouldRescaleVisible) {
            rescaleVisibleParts();
        }
    }
    mBlockChildAdd = false;
    updateSizes(isVisible());
}

QWidget *Splitter::replaceWidget(int index, QWidget *widget)
{
    if (widget == nullptr) {
        RE_LOG_ERROR("Widget is null");
        return nullptr;
    }
    if (!indexInRange(index)) {
        RE_LOG_ERROR("Invalid index: %i", index);
        return nullptr;
    }
    if (widget->findChildren<Splitter *>().contains(this)) {
        RE_LOG_ERROR("Can't insert parent widget");
        return nullptr;
    }
    if (indexOf(widget) != -1) {
        RE_LOG_ERROR("Widget not found");
        return nullptr;
    }
    mBlockChildAdd = true;
    bool willShow = shouldShow(mWidgets[index]);
    widget->setParent(this);
    std::swap(mWidgets[index], widget);
    if (willShow) {
        mWidgets[index]->show();
    }
    updateSizes(isVisible());
    mBlockChildAdd = false;
    return widget;
}

void Splitter::insertAllWidgetsFrom(Splitter *other, int index, int offset)
{
    if (other->count() > 1 && other->orientation() != orientation()) {
        RE_LOG_ERROR("Orientation mismatch");
        return;
    }
    if (!other) {
        RE_LOG_ERROR("Other splitter is null");
        return;
    }
    if (other->findChildren<Splitter *>().contains(this)) {
        RE_LOG_ERROR("Can't insert parent widget");
        return;
    }
    if (other->count() == 0) {
        return;
    }
    if (!indexInRange(index)) {
        index = count() - 1;
        offset = 1;
    }else if (offset < 0 || offset > 1) {
        offset = 1;
    }
    double scaleOtherPartsFactor = 1.0;
    bool shouldRescaleVisible = false;
    if (equalParts()) {
        double myPart = double(count()) / (count() + other->count());
        scaleParts(myPart);
        scaleOtherPartsFactor = 1.0 - myPart;
    } else {
        const int visibleIndex = findNextVisibleIndex(index, offset);
        scaleOtherPartsFactor = mParts[visibleIndex] / 2;
        mParts[visibleIndex] /= 2;
        shouldRescaleVisible = true;
    }
    mBlockChildAdd = true;
    auto otherWidgets = other->mWidgets;
    auto otherParts = other->mParts;
    for (int i = 0; i < otherWidgets.size(); i++) {
        mWidgets.insert(index + offset, otherWidgets[i]);
        mParts.insert(index + offset, otherParts[i] * scaleOtherPartsFactor);
        otherWidgets[i]->setParent(this);
        if (shouldShow(otherWidgets[i])) {
            otherWidgets[i]->show();
        }
        index++;
    }
    mBlockChildAdd = false;
    if (shouldRescaleVisible) {
        rescaleVisibleParts();
    }
    updateSizes(isVisible());
}

QWidget *Splitter::removeWidget(int index)
{
    if (!indexInRange(index)) {
        RE_LOG_ERROR("Index is out of range");
        return nullptr;
    }
    QWidget *removedWidget = mWidgets[index];
    mWidgets.removeAt(index);
    mParts.removeAt(index);
    rescaleParts();
    updateSizes(isVisible());
    return removedWidget;
}

int Splitter::indexOf(QWidget *w) const
{
    return mWidgets.indexOf(w);
}

int Splitter::count() const
{
    return mWidgets.size();
}

QWidget *Splitter::widget(int index) const
{
    if (!indexInRange(index)) {
        return nullptr;
    }
    return mWidgets[index];
}

void Splitter::setParts(QList<double> parts)
{
    if (mWidgets.size() != parts.size()) {
        RE_LOG_ERROR("parts count mismatch: %i widgets != %i new parts",
                     mWidgets.size(), parts.size());
        return;
    }
    mParts = parts;
    rescaleParts();
    updateSizes(isVisible());
}

QList<double> Splitter::parts() const
{
    return mParts;
}

void Splitter::setOrientation(Qt::Orientation o)
{
    mOrientation = o;
}

Qt::Orientation Splitter::orientation() const
{
    return mOrientation;
}

void Splitter::moveSplitter(int handleIndex, int pos)
{
    if (handleIndex < 0 || handleIndex >= mHandles.size()) {
        RE_LOG_ERROR("Invalid handle index");
        return;
    }
    QList<double> visibleParts;
    QMap<int,int> partIndexMap;
    for (int i = 0; i < mWidgets.size(); i++) {
        if (!mWidgets[i]->isHidden()) {
            partIndexMap[visibleParts.size()] = i;
            visibleParts.push_back(mParts[i]);
        }
    }
    double scaleFactor = rescaleParts(&visibleParts);
    if (handleIndex >= visibleParts.size() - 1) {
        RE_LOG_ERROR("Handle index is out of range");
        return;
    }

    const double min = std::accumulate(visibleParts.begin(),
                                       visibleParts.begin() + handleIndex,
                                       0.0);
    const double max = min + visibleParts[handleIndex] + visibleParts[handleIndex + 1];

    int size = (mOrientation == Qt::Horizontal ? this->size().width() : this->size().height());
    double minSizePart = double(minSizePx) / size;
    if (max - minSizePart <= min + minSizePart) {
        return;
    }

    double value = std::clamp(double(pos) / size, min + minSizePart, max - minSizePart);

    visibleParts[handleIndex] = value - min;
    visibleParts[handleIndex + 1] = max - value;

    mParts[partIndexMap[handleIndex]] = visibleParts[handleIndex] / scaleFactor;
    mParts[partIndexMap[handleIndex + 1]] = visibleParts[handleIndex + 1] / scaleFactor;

    updateSizes(isVisible());
    emit splitterMoved();
}

//============================================================================
bool Splitter::hasVisibleContent() const
{
	// TODO Cache or precalculate this to speed up
	for (int i = 0; i < count(); ++i)
	{
		if (!widget(i)->isHidden())
		{
			return true;
		}
	}
	return false;
}


//============================================================================
QWidget* Splitter::firstWidget() const
{
	return (count() > 0) ? widget(0) : nullptr;
}


//============================================================================
QWidget* Splitter::lastWidget() const
{
	return (count() > 0) ? widget(count() - 1) : nullptr;
}

bool Splitter::event(QEvent *e)
{
    switch (e->type()) {
    case QEvent::Hide:
        // Reset firstShow to false here since things can be done to the splitter in between
        if (!mFirstShow)
            mFirstShow = true;
        break;
    case QEvent::Show:
        if (!mFirstShow)
            break;
        mFirstShow = false;
        Q_FALLTHROUGH();
    case QEvent::HideToParent:
    case QEvent::ShowToParent:
    case QEvent::LayoutRequest:
        updateSizes(isVisible());
        break;
    default:
        break;
    }
    return QWidget::event(e);
}

void Splitter::childEvent(QChildEvent *c)
{
    if (!c->child()->isWidgetType()) {
        return;
    }
    if (c->polished()) {
        QWidget *w = static_cast<QWidget *>(c->child());
        if (!mBlockChildAdd && !w->isWindow() && shouldShow(w)) {
            w->show();
        }
    } else if (c->removed()) {
        QObject *child = c->child();
        for (int i = 0; i < count(); ++i) {
            if (mWidgets[i] == child) {
                removeWidget(i);
                return;
            }
        }
    }
}

void Splitter::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    updateSizes(isVisible());
}

bool Splitter::indexInRange(int index) const
{
    return index >= 0 && index < count();
}

int Splitter::findNextVisibleIndex(int fromIndex, int insertOffset) const
{
    int res = fromIndex;
    int step = (insertOffset == 1 ? -1 : 1);
    while (indexInRange(res) && mWidgets[res]->isHidden()) {
        res += step;
    }
    if (!indexInRange(res)) {
        res = fromIndex - step;
        while (indexInRange(res) && mWidgets[res]->isHidden()) {
            res -= step;
        }
        if (!indexInRange(res)) {
            RE_LOG_ERROR("Next visible widget not found");
            return fromIndex;
        }
    }
    return res;
}

bool Splitter::equalParts() const
{
    for (int i = 1; i < mParts.size(); i++) {
        if (std::fabs(mParts[0] - mParts[i]) > 0.0001) {
            return false;
        }
    }
    return true;
}

double Splitter::getEqualPart() const
{
    if (mParts.empty()) {
        return 1.0;
    }
    return mParts[0];
}

void Splitter::scaleParts(double factor)
{
    for (auto &p : mParts) {
        p *= factor;
    }
}

double Splitter::rescaleParts(QList<double> *parts)
{
    if (!parts) {
        parts = &mParts;
    }
    if (parts->empty()) {
        return 1.0;
    }
    const double sum = std::accumulate(parts->begin(), parts->end(), 0.0);
    const double factor = 1 / sum;
    for (auto &p : *parts) {
        p *= factor;
        if (!std::isnormal(p) || p <= 0) {
            std::fill(parts->begin(), parts->end(), 1.0 / parts->size());
            return 1.0;
        }
    }
    return factor;
}

void Splitter::rescaleVisibleParts()
{
    if (mParts.size() != mWidgets.size()) {
        RE_LOG_ERROR("Parts and widgets count mismatch");
        return;
    }
    QList<double> visibleParts;
    for (int i = 0; i < mWidgets.size(); i++) {
        if (!mWidgets[i]->isHidden()) {
            visibleParts.push_back(mParts[i]);
        }
    }
    if (visibleParts.empty()) {
        return;
    }
    rescaleParts(&visibleParts);
    for (int i = 0; i < mWidgets.size(); i++) {
        if (!mWidgets[i]->isHidden()) {
            mParts[i] = visibleParts.front();
            visibleParts.pop_front();
        }
    }
}

void Splitter::updateSizes(bool update)
{
    if (mWidgets.size() == 0) {
        return;
    }
    if (!update) {
        mFirstShow = true;
        return;
    }
    if (mParts.size() != mWidgets.size()) {
        RE_LOG_ERROR("parts/widgets count mismatch (%i parts != %i widgets)",
                     mParts.size(),
                     mWidgets.size());
        return;
    }

    QList<QWidget *> visibleWidgets;
    QList<double> visibleParts;
    for (int i = 0; i < mWidgets.size(); i++) {
        if (!mWidgets[i]->isHidden()) {
            visibleWidgets.push_back(mWidgets[i]);
            visibleParts.push_back(mParts[i]);
        }
    }

    rescaleParts(&visibleParts);

    while (mHandles.size() > 0 && mHandles.size() > visibleWidgets.size() - 1) {
        delete mHandles.back();
        mHandles.pop_back();
    }
    while (mHandles.size() < visibleWidgets.size() - 1) {
        SplitterHandle *handle = new SplitterHandle(mOrientation, mHandles.size(), this);
        mHandles.push_back(handle);
    }

    int size = 0;
    switch (mOrientation) {
    case Qt::Horizontal: size = this->width(); break;
    case Qt::Vertical: size = this->height(); break;
    }
    int pos = 0;
    for (int i = 0; i < visibleWidgets.size(); i++) {
        int nextPos = pos + std::lround(visibleParts[i] * size);
        bool last = (i == visibleWidgets.size() - 1);
        if (last) {
            nextPos = size;
        } else if (i == 0) {
            nextPos += handleWidth / 2;
        }
        int length = nextPos - pos - (last ? 0 : handleWidth);
        switch (mOrientation) {
        case Qt::Horizontal:
            visibleWidgets[i]->setGeometry(pos, 0, length, this->height());
            break;
        case Qt::Vertical:
            visibleWidgets[i]->setGeometry(0, pos, this->width(), length);
            break;
        }
        if (!last) {
            mHandles[i]->setHandlePos(nextPos - handleWidth);
        }
        pos = nextPos;
    }
    for (auto w : mHandles) {
        w->raise();
    }
}

bool Splitter::shouldShow(QWidget *w)
{
    if (!this->isVisible()) {
        return false;
    }
    if (w->isHidden() && w->testAttribute(Qt::WA_WState_ExplicitShowHide)) {
        return false;
    }
    return true;
}

SplitterHandle::SplitterHandle(Qt::Orientation o, int handleIndex, Splitter *splitter)
    : QWidget(splitter), mSplitter(splitter), mIndex(handleIndex)
{
    setOrientation(o);
}

void SplitterHandle::setOrientation(Qt::Orientation o)
{
    mOrientation = o;
    setCursor(mOrientation == Qt::Horizontal ? Qt::SplitHCursor : Qt::SplitVCursor);
}

Qt::Orientation SplitterHandle::orientation() const
{
    return mOrientation;
}

void SplitterHandle::setHandlePos(int pos)
{
    const int marginForGrabArea = 2;
    switch (mOrientation) {
    case Qt::Horizontal:
        setGeometry(pos - marginForGrabArea, 0,
                    marginForGrabArea * 2 + mSplitter->handleWidth, mSplitter->height());
        setContentsMargins(marginForGrabArea, 0, marginForGrabArea, 0);
        break;
    case Qt::Vertical:
        setGeometry(0, pos - marginForGrabArea,
                    mSplitter->width(), marginForGrabArea * 2 + mSplitter->handleWidth);
        setContentsMargins(0, marginForGrabArea, 0, marginForGrabArea);
        break;
    }
    setAttribute(Qt::WA_MouseNoMask, true);
    setMask(QRegion(contentsRect()));
}

void SplitterHandle::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QStyleOption opt(0);
    opt.rect = contentsRect();
    opt.palette = palette();
    if (orientation() == Qt::Horizontal) {
        opt.state = QStyle::State_Horizontal;
    } else {
        opt.state = QStyle::State_None;
    }
    if (mHover) {
        opt.state |= QStyle::State_MouseOver;
    }
    if (mPressed) {
        opt.state |= QStyle::State_Sunken;
    }
    parentWidget()->style()->drawControl(QStyle::CE_Splitter, &opt, &p, mSplitter);
}

void SplitterHandle::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        mMouseOffset = (mOrientation == Qt::Horizontal ? e->pos().x() : e->pos().y());
        mPressed = true;
        update();
    }
}

void SplitterHandle::mouseMoveEvent(QMouseEvent *e)
{
    if (!(e->buttons() & Qt::LeftButton))
        return;
    QPoint newPos = parentWidget()->mapFromGlobal(e->globalPos());
    int pos = (mOrientation == Qt::Horizontal ? newPos.x() : newPos.y()) - mMouseOffset;
    mSplitter->moveSplitter(mIndex, pos);
}

void SplitterHandle::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        mPressed = false;
        update();
    }
}

bool SplitterHandle::event(QEvent *e)
{
    switch(e->type()) {
    case QEvent::HoverEnter:
        mHover = true;
        update();
        break;
    case QEvent::HoverLeave:
        mHover = false;
        update();
        break;
    default:
        break;
    }
    return QWidget::event(e);
}

} // namespace ads

//---------------------------------------------------------------------------
// EOF DockSplitter.cpp
