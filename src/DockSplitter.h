#ifndef DockSplitterH
#define DockSplitterH
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
/// \file   DockSplitter.h
/// \author Uwe Kindler
/// \date   24.03.2017
/// \brief  Declaration of CDockSplitter
//============================================================================

//============================================================================
//                                   INCLUDES
//============================================================================
#include <QWidget>

#include "ads_globals.h"

namespace ads {

class SplitterHandle;

class ADS_EXPORT Splitter : public QWidget
{
    Q_OBJECT
public:
    Splitter(Qt::Orientation orient, QWidget *parent = Q_NULLPTR);

    void insertWidget(QWidget *widget, int index, int offset);
    QWidget *replaceWidget(int index, QWidget *widget);

    void insertAllWidgetsFrom(Splitter *other, int index, int offset);

    QWidget *removeWidget(int index);

    int indexOf(QWidget *w) const;
    int count() const;
    QWidget *widget(int index) const;

    void setParts(QList<double> parts);
    QList<double> parts() const;

    void setOrientation(Qt::Orientation o);
    Qt::Orientation orientation() const;

    void moveSplitter(int handleIndex, int pos);

    constexpr static int minSizePx = 40;
    constexpr static int handleWidth = 2;

    /**
	 * Returns true, if any of the internal widgets is visible
     */
    bool hasVisibleContent() const;

    /**
	 * Returns first widget or nullptr if splitter is empty
     */
    QWidget *firstWidget() const;

    /**
	 * Returns last widget of nullptr is splitter is empty
     */
    QWidget *lastWidget() const;

signals:
    void splitterMoved();

protected:
    bool event(QEvent *e) override;
    void childEvent(QChildEvent *c) override;
    void resizeEvent(QResizeEvent *e) override;

private:
    bool indexInRange(int index) const;
    int findNextVisibleIndex(int fromIndex, int insertOffset) const;

    bool equalParts() const;
    double getEqualPart() const;
    void scaleParts(double factor);

    /** returns factor */
    double rescaleParts(QList<double> *parts = nullptr);

    /** scales visible part to one leaving hidden untouched */
    void rescaleVisibleParts();

    void updateSizes(bool update);
    bool shouldShow(QWidget *w);

    int mVisibleContentCount = 0;
    Qt::Orientation mOrientation = Qt::Horizontal;
    QList<QWidget *> mWidgets;
    QList<SplitterHandle *> mHandles;
    QList<double> mParts;
    bool mFirstShow = true;
    bool mBlockChildAdd = false;
};

class ADS_EXPORT SplitterHandle : public QWidget
{
    Q_OBJECT
public:
    SplitterHandle(Qt::Orientation o, int handleIndex, Splitter *splitter);

    void setOrientation(Qt::Orientation o);
    Qt::Orientation orientation() const;

    void setHandlePos(int pos);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    bool event(QEvent *e) override;

private:
    Splitter *mSplitter;
    int mIndex = 0;
    Qt::Orientation mOrientation = Qt::Horizontal;

    bool mHover = false;;
    int mMouseOffset = 0;
    bool mPressed = false;
};

} // namespace ads

//---------------------------------------------------------------------------
#endif // DockSplitterH
