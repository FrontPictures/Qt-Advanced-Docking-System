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
/// \file   DockAreaTitleBar.cpp
/// \author Uwe Kindler
/// \date   12.10.2018
/// \brief  Implementation of CDockAreaTitleBar class
//============================================================================

//============================================================================
//                                   INCLUDES
//============================================================================
#include "DockAreaTitleBar.h"

#include <QPushButton>
#include <QToolButton>
#include <QBoxLayout>
#include <QStyle>
#include <QMenu>
#include <QScrollArea>
#include <QMouseEvent>
#include <QDebug>
#include <QPointer>

#include "DockAreaTitleBar_p.h"
#include "ads_globals.h"
#include "FloatingDockContainer.h"
#include "FloatingDragPreview.h"
#include "DockAreaWidget.h"
#include "DockOverlay.h"
#include "DockManager.h"
#include "DockWidget.h"
#include "DockWidgetTab.h"
#include "DockAreaTabBar.h"
#include "IconProvider.h"
#include "DockComponentsFactory.h"

#include <iostream>

#define RE_LOG_ENABLE
//#define RE_LOG_DEBUG_ENABLE
#define RE_LOG_INSTANCE_NAME_C_STR "CDockAreaTitleBar"

#include "NMLogger.h"

namespace ads
{

/**
 * Private data class of CDockAreaTitleBar class (pimpl)
 */
struct DockAreaTitleBarPrivate
{
	CDockAreaTitleBar* _this;
    QPointer<tTitleBarButton> GroupButton;
	QPointer<tTitleBarButton> TabsMenuButton;
	QPointer<tTitleBarButton> UndockButton;
	QPointer<tTitleBarButton> CloseButton;
	QBoxLayout* Layout;
    QWidget *buttonsContainer = nullptr;
    QBoxLayout* buttonsContainerLayout = nullptr;
	CDockAreaWidget* DockArea;
	CDockAreaTabBar* TabBar;
	bool MenuOutdated = true;
	QMenu* TabsMenu;
	QList<tTitleBarButton*> DockWidgetActionsButtons;

	QPoint DragStartMousePos;
	eDragState DragState = DraggingInactive;
	IFloatingWidget* FloatingWidget = nullptr;


	/**
	 * Private data constructor
	 */
	DockAreaTitleBarPrivate(CDockAreaTitleBar* _public);

	/**
	 * Creates the title bar close and menu buttons
	 */
	void createButtons();

	/**
	 * Creates the internal TabBar
	 */
	void createTabBar();

    /**
     * Creates menu with close and detach actions
     */
    QMenu *createGenaralGroupMenu(QWidget *parent);

	/**
	 * Convenience function for DockManager access
	 */
	CDockManager* dockManager() const
	{
		return DockArea->dockManager();
	}

	/**
	 * Returns true if the given config flag is set
	 * Convenience function to ease config flag testing
	 */
	static bool testConfigFlag(CDockManager::eConfigFlag Flag)
	{
		return CDockManager::testConfigFlag(Flag);
	}

	/**
	 * Test function for current drag state
	 */
	bool isDraggingState(eDragState dragState) const
	{
		return this->DragState == dragState;
	}


	/**
	 * Starts floating
	 */
	void startFloating(const QPoint& Offset);

	/**
	 * Makes the dock area floating
	 */
	IFloatingWidget* makeAreaFloating(const QPoint& Offset, eDragState DragState);
};// struct DockAreaTitleBarPrivate

//============================================================================
DockAreaTitleBarPrivate::DockAreaTitleBarPrivate(CDockAreaTitleBar* _public) :
	_this(_public)
{

}


//============================================================================
void DockAreaTitleBarPrivate::createButtons()
{
    QSizePolicy ButtonSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    QSize buttonSize(24, 24);

    // using additional widget to amke possible to styling titlle buttons are
    buttonsContainer = new QWidget;
    buttonsContainer->setObjectName("buttonsContainer");
    buttonsContainerLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    buttonsContainerLayout->setContentsMargins(0, 0, 0, 0);
    buttonsContainer->setLayout(buttonsContainerLayout);
    Layout->addWidget(buttonsContainer);

    buttonsContainerLayout->addStretch();

	// Tabs menu button
	TabsMenuButton = new CTitleBarButton(testConfigFlag(CDockManager::DockAreaHasTabsMenuButton));
	TabsMenuButton->setObjectName("tabsMenuButton");
	TabsMenuButton->setAutoRaise(true);
	TabsMenuButton->setPopupMode(QToolButton::InstantPopup);
	internal::setButtonIcon(TabsMenuButton, QStyle::SP_TitleBarUnshadeButton, ads::DockAreaMenuIcon);
	QMenu* TabsMenu = new QMenu(TabsMenuButton);
#ifndef QT_NO_TOOLTIP
	TabsMenu->setToolTipsVisible(true);
#endif
	_this->connect(TabsMenu, SIGNAL(aboutToShow()), SLOT(onTabsMenuAboutToShow()));
	TabsMenuButton->setMenu(TabsMenu);
	internal::setToolTip(TabsMenuButton, QObject::tr("List All Tabs"));
    TabsMenuButton->setFixedSize(buttonSize);
    buttonsContainerLayout->addWidget(TabsMenuButton, 0);
	_this->connect(TabsMenuButton->menu(), SIGNAL(triggered(QAction*)),
        SLOT(onTabsMenuActionTriggered(QAction*)));

    if (testConfigFlag(CDockManager::DockAreaHasGroupMenuButton)) {
        CDockManager::setConfigFlag(CDockManager::DockAreaHasUndockButton, false);
        CDockManager::setConfigFlag(CDockManager::DockAreaHasCloseButton, false);
    }

    //Group button
    GroupButton = new CTitleBarButton(testConfigFlag(CDockManager::DockAreaHasGroupMenuButton));
    GroupButton->setObjectName("tabsGroupButton");
    GroupButton->setAutoRaise(true);
    GroupButton->setPopupMode(QToolButton::InstantPopup);
    internal::setButtonIcon(GroupButton, QStyle::SP_ToolBarHorizontalExtensionButton, ads::DockAreaGroupMenuIcon);
    GroupButton->setFixedSize(buttonSize);
    buttonsContainerLayout->addWidget(GroupButton);

    auto *menu = createGenaralGroupMenu(GroupButton);
    GroupButton->setMenu(menu);

    // Undock button
    UndockButton = new CTitleBarButton(testConfigFlag(CDockManager::DockAreaHasUndockButton));
    UndockButton->setObjectName("detachGroupButton");
    UndockButton->setAutoRaise(true);
    internal::setToolTip(UndockButton, QObject::tr("Detach Group"));
    internal::setButtonIcon(UndockButton, QStyle::SP_TitleBarNormalButton, ads::DockAreaUndockIcon);
    UndockButton->setFixedSize(buttonSize);
    buttonsContainerLayout->addWidget(UndockButton);
    _this->connect(UndockButton, SIGNAL(clicked()), SLOT(onUndockButtonClicked()));

    // Close button
    CloseButton = new CTitleBarButton(testConfigFlag(CDockManager::DockAreaHasCloseButton));
    CloseButton->setObjectName("dockAreaCloseButton");
    CloseButton->setAutoRaise(true);
    internal::setButtonIcon(UndockButton, QStyle::SP_TitleBarCloseButton, ads::DockAreaCloseIcon);
    if (testConfigFlag(CDockManager::DockAreaCloseButtonClosesTab))
    {
        internal::setToolTip(CloseButton, QObject::tr("Close Active Tab"));
    }
    else
    {
        internal::setToolTip(CloseButton, QObject::tr("Close Group"));
    }
    CloseButton->setFixedSize(buttonSize);
    buttonsContainerLayout->addWidget(CloseButton);
    _this->connect(CloseButton, SIGNAL(clicked()), SLOT(onCloseButtonClicked()));
}


//============================================================================
void DockAreaTitleBarPrivate::createTabBar()
{
	TabBar = componentsFactory()->createDockAreaTabBar(DockArea);
    TabBar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
	Layout->addWidget(TabBar);
	_this->connect(TabBar, SIGNAL(tabClosed(int)), SLOT(markTabsMenuOutdated()));
	_this->connect(TabBar, SIGNAL(tabOpened(int)), SLOT(markTabsMenuOutdated()));
	_this->connect(TabBar, SIGNAL(tabInserted(int)), SLOT(markTabsMenuOutdated()));
	_this->connect(TabBar, SIGNAL(removingTab(int)), SLOT(markTabsMenuOutdated()));
	_this->connect(TabBar, SIGNAL(tabMoved(int, int)), SLOT(markTabsMenuOutdated()));
	_this->connect(TabBar, SIGNAL(currentChanged(int)), SLOT(onCurrentTabChanged(int)));
	_this->connect(TabBar, SIGNAL(tabBarClicked(int)), SIGNAL(tabBarClicked(int)));
	_this->connect(TabBar, SIGNAL(elidedChanged(bool)), SLOT(markTabsMenuOutdated()));
}


//============================================================================
QMenu *DockAreaTitleBarPrivate::createGenaralGroupMenu(QWidget *parent)
{
    QMenu *menu = new QMenu(parent);
    auto *detachAct = menu->addAction("Detach Group");
    auto *closeAct = menu->addAction("Close Group");
    _this->connect(detachAct, &QAction::triggered, [=](bool) {
        _this->onUndockButtonClicked();
    });
    _this->connect(closeAct, &QAction::triggered, [=](bool) {
        _this->onCloseButtonClicked();
    });
    return menu;
}


//============================================================================
IFloatingWidget* DockAreaTitleBarPrivate::makeAreaFloating(const QPoint& Offset, eDragState DragState)
{
	QSize Size = DockArea->size();
	this->DragState = DragState;
	bool OpaqueUndocking = CDockManager::testConfigFlag(CDockManager::OpaqueUndocking) ||
		(DraggingFloatingWidget != DragState);
	CFloatingDockContainer* FloatingDockContainer = nullptr;
	IFloatingWidget* FloatingWidget;
	if (OpaqueUndocking)
	{
		FloatingWidget = FloatingDockContainer = new CFloatingDockContainer(DockArea);
	}
	else
	{
		auto w = new CFloatingDragPreview(DockArea);
		QObject::connect(w, &CFloatingDragPreview::draggingCanceled, [=]()
		{
			this->DragState = DraggingInactive;
		});
		FloatingWidget = w;
	}

    FloatingWidget->startFloating(Offset, Size, DragState, nullptr);
    if (FloatingDockContainer)
    {
		auto TopLevelDockWidget = FloatingDockContainer->topLevelDockWidget();
		if (TopLevelDockWidget)
		{
			TopLevelDockWidget->emitTopLevelChanged(true);
		}
    }

	return FloatingWidget;
}


//============================================================================
void DockAreaTitleBarPrivate::startFloating(const QPoint& Offset)
{
	FloatingWidget = makeAreaFloating(Offset, DraggingFloatingWidget);
}


//============================================================================
CDockAreaTitleBar::CDockAreaTitleBar(CDockAreaWidget* parent) :
	QFrame(parent),
	d(new DockAreaTitleBarPrivate(this))
{
	d->DockArea = parent;

	setObjectName("dockAreaTitleBar");
	d->Layout = new QBoxLayout(QBoxLayout::LeftToRight);
    d->Layout->setContentsMargins(0, 0, 0, 0);
    d->Layout->setSpacing(0);
	setLayout(d->Layout);
	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

	d->createTabBar();
	d->createButtons();

    setFocusPolicy(Qt::NoFocus);
}


//============================================================================
CDockAreaTitleBar::~CDockAreaTitleBar()
{
    if (!d->GroupButton.isNull())
    {
        delete d->GroupButton;
    }
	if (!d->CloseButton.isNull())
	{
		delete d->CloseButton;
	}

	if (!d->TabsMenuButton.isNull())
	{
		delete d->TabsMenuButton;
	}

	if (!d->UndockButton.isNull())
	{
		delete d->UndockButton;
	}
	delete d;
}


//============================================================================
CDockAreaTabBar* CDockAreaTitleBar::tabBar() const
{
	return d->TabBar;
}

//============================================================================
void CDockAreaTitleBar::markTabsMenuOutdated()
{
	if(DockAreaTitleBarPrivate::testConfigFlag(CDockManager::DockAreaDynamicTabsMenuButtonVisibility))
	{
		bool hasElidedTabTitle = false;
		for (int i = 0; i < d->TabBar->count(); ++i)
		{
			if (!d->TabBar->isTabOpen(i))
			{
				continue;
			}
			CDockWidgetTab* Tab = d->TabBar->tab(i);
			if(Tab->isTitleElided())
			{
				hasElidedTabTitle = true;
				break;
			}
		}
		bool visible = (hasElidedTabTitle && (d->TabBar->count() > 1));
		QMetaObject::invokeMethod(d->TabsMenuButton, "setVisible", Qt::QueuedConnection, Q_ARG(bool, visible));
	}
	d->MenuOutdated = true;
}

//============================================================================
void CDockAreaTitleBar::onTabsMenuAboutToShow()
{
	if (!d->MenuOutdated)
	{
		return;
	}

	QMenu* menu = d->TabsMenuButton->menu();
	menu->clear();
	for (int i = 0; i < d->TabBar->count(); ++i)
	{
		if (!d->TabBar->isTabOpen(i))
		{
			continue;
		}
		auto Tab = d->TabBar->tab(i);
		QAction* Action = menu->addAction(Tab->icon(), Tab->text());
		internal::setToolTip(Action, Tab->toolTip());
		Action->setData(i);
	}

	d->MenuOutdated = false;
}


//============================================================================
void CDockAreaTitleBar::onCloseButtonClicked()
{
    RE_LOG_DEBUG("CDockAreaTitleBar::onCloseButtonClicked");
	if (d->testConfigFlag(CDockManager::DockAreaCloseButtonClosesTab))
	{
		d->TabBar->closeTab(d->TabBar->currentIndex());
	}
	else
	{
		d->DockArea->closeArea();
	}
}


//============================================================================
void CDockAreaTitleBar::onUndockButtonClicked()
{
	if (d->DockArea->features().testFlag(CDockWidget::DockWidgetFloatable))
	{
		d->makeAreaFloating(mapFromGlobal(QCursor::pos()), DraggingInactive);
	}
}


//============================================================================
void CDockAreaTitleBar::onTabsMenuActionTriggered(QAction* Action)
{
	int Index = Action->data().toInt();
	d->TabBar->setCurrentIndex(Index);
	emit tabBarClicked(Index);
}


//============================================================================
void CDockAreaTitleBar::updateDockWidgetActionsButtons()
{
    auto currentTab = d->TabBar->currentTab();
    if (!currentTab) {
        return;
    }
    CDockWidget* DockWidget = currentTab->dockWidget();
	if (!d->DockWidgetActionsButtons.isEmpty())
	{
		for (auto Button : d->DockWidgetActionsButtons)
		{
            d->buttonsContainerLayout->removeWidget(Button);
			delete Button;
		}
		d->DockWidgetActionsButtons.clear();
	}

	auto Actions = DockWidget->titleBarActions();
	if (Actions.isEmpty())
	{
		return;
	}

	int InsertIndex = indexOf(d->TabsMenuButton);
	for (auto Action : Actions)
	{
		auto Button = new CTitleBarButton(true, this);
		Button->setDefaultAction(Action);
		Button->setAutoRaise(true);
		Button->setPopupMode(QToolButton::InstantPopup);
		Button->setObjectName(Action->objectName());
        d->buttonsContainerLayout->insertWidget(InsertIndex++, Button, 0);
		d->DockWidgetActionsButtons.append(Button);
	}
}


//============================================================================
void CDockAreaTitleBar::onCurrentTabChanged(int Index)
{
	if (Index < 0)
	{
		return;
	}

	if (d->testConfigFlag(CDockManager::DockAreaCloseButtonClosesTab))
	{
		CDockWidget* DockWidget = d->TabBar->tab(Index)->dockWidget();
		d->CloseButton->setEnabled(DockWidget->features().testFlag(CDockWidget::DockWidgetClosable));
	}

	updateDockWidgetActionsButtons();
}


//============================================================================
QAbstractButton* CDockAreaTitleBar::button(TitleBarButton which) const
{
	switch (which)
	{
	case TitleBarButtonTabsMenu: return d->TabsMenuButton;
	case TitleBarButtonUndock: return d->UndockButton;
	case TitleBarButtonClose: return d->CloseButton;
	default:
		return nullptr;
	}
}


//============================================================================
void CDockAreaTitleBar::setVisible(bool Visible)
{
	Super::setVisible(Visible);
    markTabsMenuOutdated();
}

void CDockAreaTitleBar::setGroupMenu(QMenu *widgetsMenu)
{
    if (d->GroupButton) {
        auto oldMenu = d->GroupButton->menu();
        bool needOpen = false;
        if (oldMenu) {
            needOpen = oldMenu->isVisible();
            oldMenu->close();
            auto list = oldMenu->actions();
            qDeleteAll(list);
            oldMenu->deleteLater();
        }
        auto *menu = new QMenu(d->GroupButton);
        widgetsMenu->setTitle(tr("Insert widget"));
        menu->addMenu(widgetsMenu);
        menu->addSeparator();
        menu->addActions(d->createGenaralGroupMenu(d->GroupButton)->actions());
        d->GroupButton->setMenu(menu);
        if (needOpen) {
            d->GroupButton->showMenu();
        }
    }
}


//============================================================================
void CDockAreaTitleBar::mousePressEvent(QMouseEvent* ev)
{
	if (ev->button() == Qt::LeftButton)
	{
		ev->accept();
		d->DragStartMousePos = ev->pos();
		d->DragState = DraggingMousePressed;

		if (CDockManager::testConfigFlag(CDockManager::FocusHighlighting))
		{
			d->TabBar->currentTab()->setFocus(Qt::OtherFocusReason);
		}
		return;
	}
	Super::mousePressEvent(ev);
}


//============================================================================
void CDockAreaTitleBar::mouseReleaseEvent(QMouseEvent* ev)
{
	if (ev->button() == Qt::LeftButton)
	{
        RE_LOG_DEBUG("CDockAreaTitleBar::mouseReleaseEvent");
		ev->accept();
		auto CurrentDragState = d->DragState;
		d->DragStartMousePos = QPoint();
		d->DragState = DraggingInactive;
		if (DraggingFloatingWidget == CurrentDragState)
		{
			d->FloatingWidget->finishDragging();
		}

		return;
	}
	Super::mouseReleaseEvent(ev);
}


//============================================================================
void CDockAreaTitleBar::mouseMoveEvent(QMouseEvent* ev)
{
	Super::mouseMoveEvent(ev);
	if (!(ev->buttons() & Qt::LeftButton) || d->isDraggingState(DraggingInactive))
	{
		d->DragState = DraggingInactive;
		return;
	}

    // move floating window
    if (d->isDraggingState(DraggingFloatingWidget))
    {
        d->FloatingWidget->moveFloating();
        return;
    }

	// If this is the last dock area in a floating dock container it does not make
	// sense to move it to a new floating widget and leave this one
	// empty
	if (d->DockArea->dockContainer()->isFloating()
	 && d->DockArea->dockContainer()->visibleDockAreaCount() == 1)
	{
		return;
	}

	// If one single dock widget in this area is not floatable then the whole
	// area is not floatable
	// If we do non opaque undocking, then we can create the floating drag
	// preview if the dock widget is movable
	auto Features = d->DockArea->features();
    if (!Features.testFlag(CDockWidget::DockWidgetFloatable)
    && !(Features.testFlag(CDockWidget::DockWidgetMovable) && !CDockManager::testConfigFlag(CDockManager::OpaqueUndocking)))
	{
		return;
	}

	int DragDistance = (d->DragStartMousePos - ev->pos()).manhattanLength();
	if (DragDistance >= CDockManager::startDragDistance())
	{
        RE_LOG_DEBUG("CDockAreaTitlBar::startFloating");
		d->startFloating(d->DragStartMousePos);
		auto Overlay = d->DockArea->dockManager()->containerOverlay();
		Overlay->setAllowedAreas(OuterDockAreas);
	}

	return;
}


//============================================================================
void CDockAreaTitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
	// If this is the last dock area in a dock container it does not make
	// sense to move it to a new floating widget and leave this one
	// empty
	if (d->DockArea->dockContainer()->isFloating() && d->DockArea->dockContainer()->dockAreaCount() == 1)
	{
		return;
	}

	if (!d->DockArea->features().testFlag(CDockWidget::DockWidgetFloatable))
	{
		return;
	}
	d->makeAreaFloating(event->pos(), DraggingInactive);
}


//============================================================================
void CDockAreaTitleBar::contextMenuEvent(QContextMenuEvent* ev)
{
	ev->accept();
	if (d->isDraggingState(DraggingFloatingWidget))
	{
		return;
	}

	QMenu Menu(this);
	auto Action = Menu.addAction(tr("Detach Group"), this, SLOT(onUndockButtonClicked()));
	Action->setEnabled(d->DockArea->features().testFlag(CDockWidget::DockWidgetFloatable));
	Menu.addSeparator();
	Action = Menu.addAction(tr("Close Group"), this, SLOT(onCloseButtonClicked()));
	Action->setEnabled(d->DockArea->features().testFlag(CDockWidget::DockWidgetClosable));
	Menu.addAction(tr("Close Other Groups"), d->DockArea, SLOT(closeOtherAreas()));
	Menu.exec(ev->globalPos());
}


//============================================================================
void CDockAreaTitleBar::insertWidget(int index, QWidget *widget)
{
    d->buttonsContainerLayout->insertWidget(index, widget);
}


//============================================================================
int CDockAreaTitleBar::indexOf(QWidget *widget) const
{
    return d->buttonsContainerLayout->indexOf(widget);
}

//============================================================================
CTitleBarButton::CTitleBarButton(bool visible, QWidget* parent)
	: tTitleBarButton(parent),
	  Visible(visible),
	  HideWhenDisabled(CDockManager::testConfigFlag(CDockManager::DockAreaHideDisabledButtons))
{
    setFocusPolicy(Qt::NoFocus);
}

//============================================================================
void CTitleBarButton::setVisible(bool visible)
{
	// 'visible' can stay 'true' if and only if this button is configured to generaly visible:
	visible = visible && this->Visible;

	// 'visible' can stay 'true' unless: this button is configured to be invisible when it is disabled and it is currently disabled:
	if (visible && HideWhenDisabled)
	{
		visible = isEnabled();
	}

	Super::setVisible(visible);
}

//============================================================================
bool CTitleBarButton::event(QEvent *ev)
{
	if (QEvent::EnabledChange == ev->type() && HideWhenDisabled)
	{
		// force setVisible() call 
		// Calling setVisible() directly here doesn't work well when button is expected to be shown first time
		QMetaObject::invokeMethod(this, "setVisible", Qt::QueuedConnection, Q_ARG(bool, isEnabled()));
	}

	return Super::event(ev);
}

//============================================================================
CSpacerWidget::CSpacerWidget(QWidget* Parent /*= 0*/) : Super(Parent)
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setStyleSheet("border: none; background: none;");
}

} // namespace ads

//---------------------------------------------------------------------------
// EOF DockAreaTitleBar.cpp
