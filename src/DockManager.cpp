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
/// \file   DockManager.cpp
/// \author Uwe Kindler
/// \date   26.02.2017
/// \brief  Implementation of CDockManager class
//============================================================================


//============================================================================
//                                   INCLUDES
//============================================================================
#include "DockWidgetTab.h"
#include "DockManager.h"

#include <algorithm>
#include <iostream>

#include <QMainWindow>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QDebug>
#include <QFile>
#include <QAction>
#include <QXmlStreamWriter>
#include <QSettings>
#include <QMenu>
#include <QApplication>

#include "FloatingDockContainer.h"
#include "DockOverlay.h"
#include "DockWidget.h"
#include "ads_globals.h"
#include "DockAreaWidget.h"
#include "IconProvider.h"
#include "DockingStateReader.h"
#include "DockGroupMenu.h"
#include "DockAreaTitleBar.h"
#include "DockFocusController.h"

#ifdef Q_OS_LINUX
#include "linux/FloatingWidgetTitleBar.h"
#endif

#define RE_LOG_ENABLE
//#define RE_LOG_DEBUG_ENABLE
#define RE_LOG_INSTANCE_NAME_C_STR "DockManager"

#include "NMLogger.h"


/**
 * Initializes the resources specified by the .qrc file with the specified base
 * name. Normally, when resources are built as part of the application, the
 * resources are loaded automatically at startup. The Q_INIT_RESOURCE() macro
 * is necessary on some platforms for resources stored in a static library.
 * Because GCC causes a linker error if we put Q_INIT_RESOURCE into the
 * loadStyleSheet() function, we place it into a function outside of the ads
 * namespace
 */
static void initResource()
{
	Q_INIT_RESOURCE(ads);
}


namespace ads
{
/**
 * Internal file version in case the structure changes internally
 */
enum eStateFileVersion
{
	InitialVersion = 0,      //!< InitialVersion
	Version1 = 1,            //!< Version1
	CurrentVersion = Version1//!< CurrentVersion
};

static CDockManager::ConfigFlags StaticConfigFlags = CDockManager::DefaultNonOpaqueConfig;

/**
 * Private data class of CDockManager class (pimpl)
 */
struct DockManagerPrivate
{
	CDockManager* _this;
	QList<CFloatingDockContainer*> FloatingWidgets;
	QList<CDockContainerWidget*> Containers;
	CDockOverlay* ContainerOverlay;
	CDockOverlay* DockAreaOverlay;
	QMap<QString, CDockWidget*> DockWidgetsMap;
	QMap<QString, QByteArray> Perspectives;
	QMap<QString, QMenu*> ViewMenuGroups;
	QMenu* ViewMenu;
    CDockGroupMenu *GroupMenu = nullptr;
	CDockManager::eViewMenuInsertionOrder MenuInsertionOrder = CDockManager::MenuAlphabeticallySorted;
	bool RestoringState = false;
	QVector<CFloatingDockContainer*> UninitializedFloatingWidgets;
	CDockFocusController* FocusController = nullptr;

	/**
	 * Private data constructor
	 */
	DockManagerPrivate(CDockManager* _public);

	/**
	 * Checks if the given data stream is a valid docking system state
	 * file.
	 */
	bool checkFormat(const QByteArray &state, int version);

	/**
	 * Restores the state
	 */
	bool restoreStateFromXml(const QByteArray &state, int version, bool Testing = internal::Restore);

    /**
     * Restores geometry of floating containers
     */
    bool restoreFloatingGeometryFromXml(const QByteArray &state, bool Testing = internal::Restore);

	/**
	 * Restore state
	 */
	bool restoreState(const QByteArray &state, int version);

    /**
     * Restore geometry of floating containers
     */
    bool restoreFloatingGeometry(const QByteArray &state, int version);

	void restoreDockWidgetsOpenState();
	void restoreDockAreasIndices();
	void emitTopLevelEvents();

	void hideFloatingWidgets()
	{
		// Hide updates of floating widgets from user
		for (auto FloatingWidget : FloatingWidgets)
		{
			FloatingWidget->hide();
		}
	}

	void markDockWidgetsDirty()
	{
		for (auto DockWidget : DockWidgetsMap)
		{
			DockWidget->setProperty("dirty", true);
		}
	}

	/**
	 * Restores the container with the given index
	 */
	bool restoreContainer(int Index, CDockingStateReader& stream, bool Testing);

	/**
	 * Loads the stylesheet
	 */
	void loadStylesheet();

	/**
	 * Adds action to menu - optionally in sorted order
	 */
	void addActionToMenu(QAction* Action, QMenu* Menu, bool InsertSorted);

    /**
     * Find CDockWidget parent of given child widget
     * Retunr nullptr if given widget is not a child of CDockWidget or is not a CDockWidget
     */
    CDockWidget *findDockWidget(QWidget *target);

    /**
     * Set widget selected if it belongs to this manager
     */
    void setSelected(CDockWidget *);
};
// struct DockManagerPrivate

//============================================================================
DockManagerPrivate::DockManagerPrivate(CDockManager* _public) :
	_this(_public)
{

}


//============================================================================
void DockManagerPrivate::loadStylesheet()
{
	initResource();
	QString Result;
	QString FileName = ":ads/stylesheets/";
	FileName += CDockManager::testConfigFlag(CDockManager::FocusHighlighting)
		? "focus_highlighting" : "default";
#ifdef Q_OS_LINUX
    FileName += "_linux";
#endif
    FileName += ".css";
	QFile StyleSheetFile(FileName);
	StyleSheetFile.open(QIODevice::ReadOnly);
	QTextStream StyleSheetStream(&StyleSheetFile);
	Result = StyleSheetStream.readAll();
	StyleSheetFile.close();
	_this->setStyleSheet(Result);
}


//============================================================================
bool DockManagerPrivate::restoreContainer(int Index, CDockingStateReader& stream, bool Testing)
{
	if (Testing)
	{
		Index = 0;
	}

	bool Result = false;
	if (Index >= Containers.count())
	{
		CFloatingDockContainer* FloatingWidget = new CFloatingDockContainer(_this);
		Result = FloatingWidget->restoreState(stream, Testing);
	}
	else
	{
        ADS_PRINT("d->Containers[i]->restoreState ");
		auto Container = Containers[Index];
		if (Container->isFloating())
		{
			Result = Container->floatingWidget()->restoreState(stream, Testing);
		}
		else
		{
			Result = Container->restoreState(stream, Testing);
		}
	}

	return Result;
}


//============================================================================
bool DockManagerPrivate::checkFormat(const QByteArray &state, int version)
{
    return restoreStateFromXml(state, version, internal::RestoreTesting);
}


//============================================================================
bool DockManagerPrivate::restoreStateFromXml(const QByteArray &state,  int version,
	bool Testing)
{
	Q_UNUSED(version);

    if (state.isEmpty())
    {
        return false;
    }
    CDockingStateReader s(state);
    s.readNextStartElement();
    if (s.name() != "AdvancedDockingSystem")
    {
    	return false;
    }
    ADS_PRINT(s.attributes().value("Version"));
    bool ok;
    int v = s.attributes().value("Version").toInt(&ok);
    if (!ok || v > CurrentVersion)
    {
    	return false;
    }
    s.setFileVersion(v);

    ADS_PRINT(s.attributes().value("UserVersion"));
    // Older files do not support UserVersion but we still want to load them so
    // we first test if the attribute exists
    if (!s.attributes().value("UserVersion").isEmpty())
    {
		v = s.attributes().value("UserVersion").toInt(&ok);
		if (!ok || v != version)
		{
			return false;
		}
    }

    bool Result = true;
#ifdef ADS_DEBUG_PRINT
    int  DockContainers = s.attributes().value("Containers").toInt();
#endif
    ADS_PRINT(DockContainers);
    int DockContainerCount = 0;
    while (s.readNextStartElement())
    {
    	if (s.name() == "Container")
    	{
    		Result = restoreContainer(DockContainerCount, s, Testing);
            if (!Result)
			{
                break;
			}
            DockContainerCount++;
    	}
    }

    if (!Testing)
    {
		// Delete remaining empty floating widgets
		int FloatingWidgetIndex = DockContainerCount - 1;
		int DeleteCount = FloatingWidgets.count() - FloatingWidgetIndex;
		for (int i = 0; i < DeleteCount; ++i)
		{
			FloatingWidgets[FloatingWidgetIndex + i]->deleteLater();
			_this->removeDockContainer(FloatingWidgets[FloatingWidgetIndex + i]->dockContainer());
		}
    }

    return Result;
}

//============================================================================
bool DockManagerPrivate::restoreFloatingGeometryFromXml(const QByteArray &state,  bool Testing)
{
    if (state.isEmpty())
    {
        return false;
    }
    CDockingStateReader s(state);
    s.readNextStartElement();
    if (s.name() != "ads-floating")
    {
        return false;
    }
    ADS_PRINT(s.attributes().value("Version"));
    bool ok;
    int v = s.attributes().value("Version").toInt(&ok);
    if (!ok || v > CurrentVersion)
    {
        return false;
    }

    s.setFileVersion(v);
    bool Result = true;
    std::map<QString, QByteArray> floatingGeo;
    while (s.readNextStartElement())
    {
        if (s.name() == "FloatingContainer")
        {
            auto title = s.attributes().value("Title").toString();
            if (!s.readNextStartElement() || s.name() != "Geometry")
            {
                RE_LOG_ERROR("No xml geometry tag");
                return false;
            }
            QByteArray GeometryString = s.readElementText(CDockingStateReader::ErrorOnUnexpectedElement).toLocal8Bit();
            floatingGeo[title] = QByteArray::fromHex(GeometryString);
            s.skipCurrentElement();
        }
    }

    for (auto fw : FloatingWidgets) {
        auto it = floatingGeo.find(fw->windowTitle());
        if (it != floatingGeo.end()) {
            fw->restoreGeometry(it->second);
        }
    }

    return Result;
}


//============================================================================
void DockManagerPrivate::restoreDockWidgetsOpenState()
{
    // All dock widgets, that have not been processed in the restore state
    // function are invisible to the user now and have no assigned dock area
    // They do not belong to any dock container, until the user toggles the
    // toggle view action the next time
    for (auto DockWidget : DockWidgetsMap)
    {
    	if (DockWidget->property(internal::DirtyProperty).toBool())
    	{
    		DockWidget->flagAsUnassigned();
            emit DockWidget->viewToggled(false);
    	}
    	else
    	{
    		DockWidget->toggleViewInternal(!DockWidget->property(internal::ClosedProperty).toBool());
    	}
    }
}


//============================================================================
void DockManagerPrivate::restoreDockAreasIndices()
{
    // Now all dock areas are properly restored and we setup the index of
    // The dock areas because the previous toggleView() action has changed
    // the dock area index
    int Count = 0;
    for (auto DockContainer : Containers)
    {
    	Count++;
    	for (int i = 0; i < DockContainer->dockAreaCount(); ++i)
    	{
    		CDockAreaWidget* DockArea = DockContainer->dockArea(i);
    		QString DockWidgetName = DockArea->property("currentDockWidget").toString();
    		CDockWidget* DockWidget = nullptr;
    		if (!DockWidgetName.isEmpty())
    		{
    			DockWidget = _this->findDockWidget(DockWidgetName);
    		}

    		if (!DockWidget || DockWidget->isClosed())
    		{
    			int Index = DockArea->indexOfFirstOpenDockWidget();
    			if (Index < 0)
    			{
    				continue;
    			}
    			DockArea->setCurrentIndex(Index);
    		}
    		else
    		{
    			DockArea->internalSetCurrentDockWidget(DockWidget);
    		}
    	}
    }
}



//============================================================================
void DockManagerPrivate::emitTopLevelEvents()
{
    // Finally we need to send the topLevelChanged() signals for all dock
    // widgets if top level changed
    for (auto DockContainer : Containers)
    {
    	CDockWidget* TopLevelDockWidget = DockContainer->topLevelDockWidget();
    	if (TopLevelDockWidget)
    	{
    		TopLevelDockWidget->emitTopLevelChanged(true);
    	}
    	else
    	{
			for (int i = 0; i < DockContainer->dockAreaCount(); ++i)
			{
				auto DockArea = DockContainer->dockArea(i);
				for (auto DockWidget : DockArea->dockWidgets())
				{
					DockWidget->emitTopLevelChanged(false);
				}
			}
    	}
    }
}


//============================================================================
bool DockManagerPrivate::restoreState(const QByteArray& State, int version)
{
	QByteArray state = State.startsWith("<?xml") ? State : qUncompress(State);
    if (!checkFormat(state, version))
    {
        ADS_PRINT("checkFormat: Error checking format!!!!!!!");
    	return false;
    }

    // Hide updates of floating widgets from use
    hideFloatingWidgets();
    markDockWidgetsDirty();

    if (!restoreStateFromXml(state, version))
    {
        ADS_PRINT("restoreState: Error restoring state!!!!!!!");
    	return false;
    }

    restoreDockWidgetsOpenState();
    restoreDockAreasIndices();
    emitTopLevelEvents();

    return true;
}

//============================================================================
bool DockManagerPrivate::restoreFloatingGeometry(const QByteArray& State, int version)
{
    QByteArray state = State.startsWith("<?xml") ? State : qUncompress(State);

    if (!restoreFloatingGeometryFromXml(state, version))
    {
        RE_LOG_ERROR("restoreState: Error restoring state!!!!!!!");
        return false;
    }

    return true;
}


//============================================================================
void DockManagerPrivate::addActionToMenu(QAction* Action, QMenu* Menu, bool InsertSorted)
{
	if (InsertSorted)
	{
		auto Actions = Menu->actions();
		auto it = std::find_if(Actions.begin(), Actions.end(),
			[&Action](const QAction* a)
			{
				return a->text().compare(Action->text(), Qt::CaseInsensitive) > 0;
			});

		if (it == Actions.end())
		{
			Menu->addAction(Action);
		}
		else
		{
			Menu->insertAction(*it, Action);
		}
	}
	else
	{
		Menu->addAction(Action);
	}
}

//============================================================================
CDockWidget *DockManagerPrivate::findDockWidget(QWidget *target)
{
    QWidget *candidate = target;
    CDockWidget *dockW = nullptr;
    while (candidate != nullptr) {
        if ((dockW = dynamic_cast<CDockWidget *>(candidate)) != nullptr) {
            return dockW;
        }

        candidate = candidate->parentWidget();
    }

    return nullptr;
}

//============================================================================
void DockManagerPrivate::setSelected(CDockWidget *widget)
{
    if (DockWidgetsMap.contains(widget->objectName())) {
        for (const auto &w : DockWidgetsMap) {
            w->setSelected(false);
        }
        RE_LOG_DEBUG("In focus: %s", qcstr(widget->objectName()));
        widget->setSelected(true);
        //_this->update();
    }
}


//============================================================================
CDockManager::CDockManager(QWidget *parent) :
	CDockContainerWidget(this, parent),
	d(new DockManagerPrivate(this))
{
    setContentsMargins(1, 0, 1, 0);
	createRootSplitter();
	QMainWindow* MainWindow = qobject_cast<QMainWindow*>(parent);
	if (MainWindow)
	{
		MainWindow->setCentralWidget(this);
	}

	d->ViewMenu = new QMenu(tr("Show View"), this);
    d->GroupMenu = new CDockGroupMenu(this);
	d->DockAreaOverlay = new CDockOverlay(this, CDockOverlay::ModeDockAreaOverlay);
	d->ContainerOverlay = new CDockOverlay(this, CDockOverlay::ModeContainerOverlay);
	d->Containers.append(this);
	d->loadStylesheet();

    //Since QEvents are propagated to child widgets first, they accept them and we cant know
    //whether any of MwpWidgets should get selected. So, we catch global focus change event
    //and iterate through focused widgets parents to see if it is one of MwpWidgets.
    //If such widget found, we can 'select' it
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *old, QWidget *now) {
        (void)old;
        auto dock = d->findDockWidget(now);
        if (dock != nullptr) {
            d->setSelected(dock);
        }
    });

    connect(this, &CDockContainerWidget::currentDockWidgetChanged, [=](CDockWidget *currentWidget) {
        d->setSelected(currentWidget);
    });
    connect(this, &CDockContainerWidget::splitterMoved, this, &CDockManager::layoutChanged);
    connect(this, &CDockContainerWidget::dockAreasAdded, this, &CDockManager::layoutChanged);

	if (CDockManager::testConfigFlag(CDockManager::FocusHighlighting))
	{
		d->FocusController = new CDockFocusController(this);
	}
}

//============================================================================
CDockManager::~CDockManager()
{
	auto FloatingWidgets = d->FloatingWidgets;
	for (auto FloatingWidget : FloatingWidgets)
	{
		delete FloatingWidget;
	}
	delete d;
}


//============================================================================
void CDockManager::registerFloatingWidget(CFloatingDockContainer* FloatingWidget)
{
	d->FloatingWidgets.append(FloatingWidget);
    connect(FloatingWidget->dockContainer(), &CDockContainerWidget::currentDockWidgetChanged, [=](CDockWidget *currentWidget) {
        d->setSelected(currentWidget);
    });
    emit floatingWidgetCreated(FloatingWidget);
    ADS_PRINT("d->FloatingWidgets.count() " << d->FloatingWidgets.count());
}


//============================================================================
void CDockManager::removeFloatingWidget(CFloatingDockContainer* FloatingWidget)
{
	d->FloatingWidgets.removeAll(FloatingWidget);
}


//============================================================================
void CDockManager::registerDockContainer(CDockContainerWidget* DockContainer)
{
    connect(DockContainer, &CDockContainerWidget::dockAreasAdded, this, &CDockManager::layoutChanged);
	d->Containers.append(DockContainer);
}


//============================================================================
void CDockManager::removeDockContainer(CDockContainerWidget* DockContainer)
{
	if (this != DockContainer)
	{
		d->Containers.removeAll(DockContainer);
	}
}


//============================================================================
CDockOverlay* CDockManager::containerOverlay() const
{
	return d->ContainerOverlay;
}


//============================================================================
CDockOverlay* CDockManager::dockAreaOverlay() const
{
	return d->DockAreaOverlay;
}


//============================================================================
const QList<CDockContainerWidget*> CDockManager::dockContainers() const
{
	return d->Containers;
}


//============================================================================
const QList<CFloatingDockContainer*> CDockManager::floatingWidgets() const
{
	return d->FloatingWidgets;
}


//============================================================================
unsigned int CDockManager::zOrderIndex() const
{
	return 0;
}


//============================================================================
QByteArray CDockManager::saveState(int version) const
{
    QByteArray xmldata;
    QXmlStreamWriter s(&xmldata);
    auto ConfigFlags = CDockManager::configFlags();
	s.setAutoFormatting(ConfigFlags.testFlag(XmlAutoFormattingEnabled));
    s.writeStartDocument();
        s.writeStartElement("AdvancedDockingSystem");
		s.writeAttribute("Version", QString::number(CurrentVersion));
		s.writeAttribute("UserVersion", QString::number(version));
		s.writeAttribute("Containers", QString::number(d->Containers.count()));
		for (auto Container : d->Containers)
		{
			Container->saveState(s);
		}

		s.writeEndElement();
    s.writeEndDocument();

    return ConfigFlags.testFlag(XmlCompressionEnabled)
    	? qCompress(xmldata, 9) : xmldata;
}


//============================================================================
bool CDockManager::restoreState(const QByteArray &state, int version)
{
	// Prevent multiple calls as long as state is not restore. This may
	// happen, if QApplication::processEvents() is called somewhere
	if (d->RestoringState)
	{
		return false;
	}

	// We hide the complete dock manager here. Restoring the state means
	// that DockWidgets are removed from the DockArea internal stack layout
	// which in turn  means, that each time a widget is removed the stack
	// will show and raise the next available widget which in turn
	// triggers show events for the dock widgets. To avoid this we hide the
	// dock manager. Because there will be no processing of application
	// events until this function is finished, the user will not see this
	// hiding
	bool IsHidden = this->isHidden();
	if (!IsHidden)
	{
		hide();
	}
	d->RestoringState = true;
	emit restoringState();
	bool Result = d->restoreState(state, version);
	d->RestoringState = false;
	if (!IsHidden)
	{
		show();
	}
	emit stateRestored();
    return Result;
}

//============================================================================
QByteArray CDockManager::saveFloatingGeometry(int version) const
{
    QByteArray xmldata;
    QXmlStreamWriter s(&xmldata);
    auto ConfigFlags = CDockManager::configFlags();
    s.setAutoFormatting(ConfigFlags.testFlag(XmlAutoFormattingEnabled));
    s.writeStartDocument();
    s.writeStartElement("ads-floating");
    s.writeAttribute("Version", QString::number(version));
    s.writeAttribute("Containers", QString::number(d->FloatingWidgets.count()));
    for (auto Floating : d->FloatingWidgets)
    {
        if(!Floating->geometry().isEmpty()) {
            s.writeStartElement("FloatingContainer");
            s.writeAttribute("Title", Floating->windowTitle());
            QByteArray Geometry = Floating->saveGeometry();
            s.writeTextElement("Geometry", Geometry.toHex(' '));
            s.writeEndElement();
        }
    }

    s.writeEndElement();
    s.writeEndDocument();

    return ConfigFlags.testFlag(XmlCompressionEnabled)
            ? qCompress(xmldata, 9) : xmldata;
}

//============================================================================
bool CDockManager::restoreFloatingGeometry(const QByteArray &state, int version)
{
    // Prevent multiple calls as long as state is not restore. This may
    // happen, if QApplication::processEvents() is called somewhere
    if (d->RestoringState)
    {
        RE_LOG_DEBUG("Still restoring");
        return false;
    }
    RE_LOG_DEBUG("Restore floating geo from: %s", state.toStdString().c_str());

    // We hide the complete dock manager here. Restoring the state means
    // that DockWidgets are removed from the DockArea internal stack layout
    // which in turn  means, that each time a widget is removed the stack
    // will show and raise the next available widget which in turn
    // triggers show events for the dock widgets. To avoid this we hide the
    // dock manager. Because there will be no processing of application
    // events until this function is finished, the user will not see this
    // hiding
    bool IsHidden = this->isHidden();
    if (!IsHidden)
    {
        hide();
    }
    d->RestoringState = true;
    emit restoringState();
    bool Result = d->restoreFloatingGeometry(state, version);
    d->RestoringState = false;
    emit stateRestored();
    if (!IsHidden)
    {
        show();
    }

    return Result;
}


//============================================================================
CFloatingDockContainer* CDockManager::addDockWidgetFloating(CDockWidget* Dockwidget, bool hide)
{
	d->DockWidgetsMap.insert(Dockwidget->objectName(), Dockwidget);
    connect(Dockwidget, &CDockWidget::viewToggled, [=](bool){
        emit layoutChanged();
    });
	CDockAreaWidget* OldDockArea = Dockwidget->dockAreaWidget();
	if (OldDockArea)
	{
		OldDockArea->removeDockWidget(Dockwidget);
	}

	Dockwidget->setDockManager(this);
	CFloatingDockContainer* FloatingWidget = new CFloatingDockContainer(Dockwidget);
	FloatingWidget->resize(Dockwidget->size());
    if (isVisible() && !hide)
	{
		FloatingWidget->show();
    }
    else if(!hide)
	{
		d->UninitializedFloatingWidgets.append(FloatingWidget);
	}
    else
    {
        Dockwidget->closeDockWidget();
        FloatingWidget->hide();
    }
    emit dockWidgetAdded(Dockwidget);
    connect(FloatingWidget->dockContainer(), &CDockContainerWidget::currentDockWidgetChanged, [=](CDockWidget *currentWidget) {
        d->setSelected(currentWidget);
    });
    return FloatingWidget;
}


//============================================================================
void CDockManager::showEvent(QShowEvent *event)
{
	Super::showEvent(event);
	if (d->UninitializedFloatingWidgets.empty())
	{
		return;
	}

	for (auto FloatingWidget : d->UninitializedFloatingWidgets)
	{
        FloatingWidget->show();
	}
	d->UninitializedFloatingWidgets.clear();
}


//============================================================================
CDockAreaWidget* CDockManager::addDockWidget(DockWidgetArea area,
	CDockWidget* Dockwidget, CDockAreaWidget* DockAreaWidget)
{
	d->DockWidgetsMap.insert(Dockwidget->objectName(), Dockwidget);
    connect(Dockwidget, &CDockWidget::viewToggled, [=](bool){
        emit layoutChanged();
    });
    auto w = CDockContainerWidget::addDockWidget(area, Dockwidget, DockAreaWidget);
    emit dockWidgetAdded(Dockwidget);
    return w;
}


//============================================================================
CDockAreaWidget* CDockManager::addDockWidgetTab(DockWidgetArea area,
	CDockWidget* Dockwidget)
{
	CDockAreaWidget* AreaWidget = lastAddedDockAreaWidget(area);
	if (AreaWidget)
	{
		return addDockWidget(ads::CenterDockWidgetArea, Dockwidget, AreaWidget);
	}
	else if (!openedDockAreas().isEmpty())
	{
		return addDockWidget(area, Dockwidget, openedDockAreas().last());
	}
	else
	{
		return addDockWidget(area, Dockwidget, nullptr);
	}
}


//============================================================================
CDockAreaWidget* CDockManager::addDockWidgetTabToArea(CDockWidget* Dockwidget,
	CDockAreaWidget* DockAreaWidget)
{
	return addDockWidget(ads::CenterDockWidgetArea, Dockwidget, DockAreaWidget);
}


//============================================================================
CDockWidget* CDockManager::findDockWidget(const QString& ObjectName) const
{
	return d->DockWidgetsMap.value(ObjectName, nullptr);
}

//============================================================================
void CDockManager::removeDockWidget(CDockWidget* Dockwidget)
{
	emit dockWidgetAboutToBeRemoved(Dockwidget);
	d->DockWidgetsMap.remove(Dockwidget->objectName());
	CDockContainerWidget::removeDockWidget(Dockwidget);
    d->GroupMenu->removeWidget(Dockwidget);
	Dockwidget->setDockManager(nullptr);
    emit dockWidgetRemoved(Dockwidget);
}

//============================================================================
void CDockManager::renameDockWidget(CDockWidget *Dockwidget, const QString &name)
{
    auto oldName = Dockwidget->windowTitle();
    if (oldName == name) {
        return;
    }
    CDockWidget *widget = nullptr;
    for (auto it = d->DockWidgetsMap.begin(); it != d->DockWidgetsMap.end(); ++it) {
        if (it.value() == Dockwidget) {
            widget = Dockwidget;
            break;
        }
    }
    if (widget) {
        // rename title of floating window if widget placed in it
        // and title equal to old name
        if (widget->isInFloatingContainer()) {
            auto container = widget->dockContainer();
            if (container != nullptr && container->isFloating()) {
                auto floatingContainer = container->floatingWidget();
                if (floatingContainer->windowTitle() == widget->windowTitle()) {
                    floatingContainer->setWindowTitle(name);
                }
            }
        }

        d->DockWidgetsMap.remove(Dockwidget->objectName());
        Dockwidget->renameDockWidget(name);
        d->DockWidgetsMap[Dockwidget->objectName()] = Dockwidget;
    }
    d->GroupMenu->renameAction(Dockwidget->getGroupName(), oldName, name);
    emit dockWidgetRenamed(Dockwidget);
}

//============================================================================
QMap<QString, CDockWidget*> CDockManager::dockWidgetsMap() const
{
	return d->DockWidgetsMap;
}


//============================================================================
void CDockManager::addPerspective(const QString& UniquePrespectiveName)
{
	d->Perspectives.insert(UniquePrespectiveName, saveState());
	emit perspectiveListChanged();
}


//============================================================================
void CDockManager::removePerspective(const QString& Name)
{
	removePerspectives({Name});
}


//============================================================================
void CDockManager::removePerspectives(const QStringList& Names)
{
	int Count = 0;
	for (auto Name : Names)
	{
		Count += d->Perspectives.remove(Name);
	}

	if (Count)
	{
		emit perspectivesRemoved();
		emit perspectiveListChanged();
	}
}


//============================================================================
QStringList CDockManager::perspectiveNames() const
{
	return d->Perspectives.keys();
}


//============================================================================
void CDockManager::openPerspective(const QString& PerspectiveName)
{
	const auto Iterator = d->Perspectives.find(PerspectiveName);
	if (d->Perspectives.end() == Iterator)
	{
		return;
	}

	emit openingPerspective(PerspectiveName);
	restoreState(Iterator.value());
	emit perspectiveOpened(PerspectiveName);
}


//============================================================================
void CDockManager::savePerspectives(QSettings& Settings) const
{
	Settings.beginWriteArray("Perspectives", d->Perspectives.size());
	int i = 0;
	for (auto it = d->Perspectives.constBegin(); it != d->Perspectives.constEnd(); ++it)
	{
		Settings.setArrayIndex(i);
		Settings.setValue("Name", it.key());
		Settings.setValue("State", it.value());
		++i;
	}
	Settings.endArray();
}


//============================================================================
void CDockManager::loadPerspectives(QSettings& Settings)
{
	d->Perspectives.clear();
	int Size = Settings.beginReadArray("Perspectives");
	if (!Size)
	{
		Settings.endArray();
		return;
	}

	for (int i = 0; i < Size; ++i)
	{
		Settings.setArrayIndex(i);
		QString Name = Settings.value("Name").toString();
		QByteArray Data = Settings.value("State").toByteArray();
		if (Name.isEmpty() || Data.isEmpty())
		{
			continue;
		}

		d->Perspectives.insert(Name, Data);
	}

	Settings.endArray();
}

//============================================================================
QAction* CDockManager::addToggleViewActionToMenu(QAction* ToggleViewAction,
	const QString& Group, const QIcon& GroupIcon)
{
	bool AlphabeticallySorted = (MenuAlphabeticallySorted == d->MenuInsertionOrder);
	if (!Group.isEmpty())
	{
		QMenu* GroupMenu = d->ViewMenuGroups.value(Group, 0);
		if (!GroupMenu)
		{
			GroupMenu = new QMenu(Group, this);
			GroupMenu->setIcon(GroupIcon);
			d->addActionToMenu(GroupMenu->menuAction(), d->ViewMenu, AlphabeticallySorted);
			d->ViewMenuGroups.insert(Group, GroupMenu);
		}
		else if (GroupMenu->icon().isNull() && !GroupIcon.isNull())
		{
			GroupMenu->setIcon(GroupIcon);
		}

		d->addActionToMenu(ToggleViewAction, GroupMenu, AlphabeticallySorted);
		return GroupMenu->menuAction();
	}
	else
	{
		d->addActionToMenu(ToggleViewAction, d->ViewMenu, AlphabeticallySorted);
		return ToggleViewAction;
	}
}


//============================================================================
QMenu* CDockManager::viewMenu() const
{
    return d->ViewMenu;
}

CDockGroupMenu *CDockManager::groupMenu()
{
    return d->GroupMenu;
}


//============================================================================
void CDockManager::setViewMenuInsertionOrder(eViewMenuInsertionOrder Order)
{
	d->MenuInsertionOrder = Order;
}


//===========================================================================
bool CDockManager::isRestoringState() const
{
	return d->RestoringState;
}


//===========================================================================
int CDockManager::startDragDistance()
{
	return QApplication::startDragDistance() * 1.5;
}


//===========================================================================
CDockManager::ConfigFlags CDockManager::configFlags()
{
	return StaticConfigFlags;
}


//===========================================================================
void CDockManager::setConfigFlags(const ConfigFlags Flags)
{
	StaticConfigFlags = Flags;
}


//===========================================================================
void CDockManager::setConfigFlag(eConfigFlag Flag, bool On)
{
	internal::setFlag(StaticConfigFlags, Flag, On);
}

//===========================================================================
bool CDockManager::testConfigFlag(eConfigFlag Flag)
{
    return configFlags().testFlag(Flag);
}


//===========================================================================
CIconProvider& CDockManager::iconProvider()
{
	static CIconProvider Instance;
	return Instance;
}


//===========================================================================
void CDockManager::notifyWidgetOrAreaRelocation(QWidget* DroppedWidget)
{
	if (d->FocusController)
	{
		d->FocusController->notifyWidgetOrAreaRelocation(DroppedWidget);
	}
}


//===========================================================================
void CDockManager::notifyFloatingWidgetDrop(CFloatingDockContainer* FloatingWidget)
{
	if (d->FocusController)
	{
		d->FocusController->notifyFloatingWidgetDrop(FloatingWidget);
	}
}


//===========================================================================
void CDockManager::setDockWidgetFocused(CDockWidget* DockWidget)
{
	if (d->FocusController)
	{
		d->FocusController->setDockWidgetFocused(DockWidget);
	}
}


} // namespace ads

//---------------------------------------------------------------------------
// EOF DockManager.cpp
