#include "DockGroupMenu.h"

#include <QMenu>

#include "DockManager.h"
#include "DockWidget.h"


namespace ads {
struct DockGroupMenuPrivate
{
/**
 * Private data class of CDockGroupMenu class (pimpl)
 */
    CDockGroupMenu*	_this			= nullptr;
    CDockManager*	DockManager	    = nullptr;

    /**
     * Private data constructor
     */
    DockGroupMenuPrivate(CDockGroupMenu* _public, CDockManager *manager)
    {
        _this = _public;
        DockManager = manager;
    }

    QList<QAction *> createActions(const std::map<QString, std::map<QString, QAction *>> &groupedActions)
    {
        QList<QAction *> res;
        for (const auto &ga : mList) {
            const auto &groupName = ga.first;
            const auto &actionList = ga.second;
            auto groupA_it = groupedActions.find(groupName);
            if (groupA_it == groupedActions.end()) {
                continue;
            }
            const auto &actionMap = groupA_it->second;
            if (groupName.isEmpty()) {
                //add as root action
                for (const auto &name : actionList) {
                    auto action_it = actionMap.find(name);
                    if (action_it != actionMap.end()) {
                        res << action_it->second;
                    }
                }
            } else {
                QAction *subaction = new QAction(groupName);
                auto submenu = new QMenu();
                subaction->setMenu(submenu);
                for (const auto &name : actionList) {
                    auto action_it = actionMap.find(name);
                    if (action_it != actionMap.end()) {
                        submenu->addAction(action_it->second);
                    }
                }
                res << subaction;
            }
        }
        return res;
    }

    std::vector<std::pair<QString, std::vector<QString>>> mList;
};


CDockGroupMenu::CDockGroupMenu(CDockManager *manager)
    : QObject(manager), d(new DockGroupMenuPrivate(this, manager))
{

}

void CDockGroupMenu::addGroup(const QString &name, int index)
{
    if (index > -1 && d->mList.size() > index) {
        d->mList.insert(d->mList.begin() + index, {name, {}});
    } else {
        d->mList.push_back({name, {}});
    }
}

void CDockGroupMenu::addWidget(const QString &groupName, CDockWidget *widget, int index)
{
    for (auto &g : d->mList) {
        auto &actionList = g.second;
        auto &group = g.first;
        if (group == groupName) {
            if (index > -1 && actionList.size() > index) {
                actionList.insert(actionList.begin() + index, widget->windowTitle());
            } else {
                actionList.push_back(widget->windowTitle());
            }
            break;
        }
    }
}

void CDockGroupMenu::removeWidget(CDockWidget *widget)
{
    for (auto &g : d->mList) {
        auto &actionList = g.second;
        const auto &actionName = widget->windowTitle();
        for (auto it = actionList.begin(); it != actionList.end(); ++it) {
            if (*it == actionName) {
                actionList.erase(it);
                return;
            }
        }
    }
}

void CDockGroupMenu::renameAction(const QString &groupName, const QString &oldName, const QString &newName)
{
    for (auto &ga : d->mList) {
        if (ga.first == groupName) {
            auto &list = ga.second;
            for (size_t i = 0; i < list.size(); ++i) {
                if (list[i] == oldName) {
                    list[i] = newName;
                    return;
                }
            }
        }
    }
}

QList<QAction *> CDockGroupMenu::getActions(CDockAreaWidget *area)
{
    if (!d->DockManager) {
        return {};
    }

    std::map<QString, std::map<QString, QAction *>> groupActions;

    const auto &widgetsList = d->DockManager->dockWidgetsMap();
    // dont add widget which already tabbed and opened
    // if widget tabbed but closed - opening it
    for (const auto &w : widgetsList) {
        QAction *action = nullptr;
        if (w->dockAreaWidget() == area) {
            if (w->isClosed()) {
                action = new QAction(w->windowTitle());
                connect(action, &QAction::triggered, this, [=]() { w->toggleView(true); });
            } else {
                continue;
            }
        } else {
            action = new QAction(w->windowTitle());
            connect(action, &QAction::triggered, this, [=]() {
                d->DockManager->addDockWidgetTabToArea(w, area);
                w->toggleView(true);
            });
        }
        const auto groupName = w->getGroupName();
        groupActions[groupName].insert({action->text(), action});
    }
    return d->createActions(groupActions);
}

}
