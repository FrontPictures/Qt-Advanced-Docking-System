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
    CDockGroupMenu::GroupFlag GroupMethod = CDockGroupMenu::GroupToSubmenu;

    /**
     * Private data constructor
     */
    DockGroupMenuPrivate(CDockGroupMenu* _public, CDockManager *manager)
    {
        _this = _public;
        DockManager = manager;
    }

    void fillMenu(QMenu *menu,
                  const std::vector<QString> &list,
                  const std::map<QString, QAction *> &actions)
    {
        for (const auto &name : list) {
            auto action_it = actions.find(name);
            if (action_it != actions.end()) {
                menu->addAction(action_it->second);
            }
        }
    }

    QMenu *createMenu(const std::map<QString, std::map<QString, QAction *>> &groupedActions)
    {
        QMenu *resMenu = new QMenu();
        for (const auto &ga : List) {
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
                        resMenu->addAction(action_it->second);
                    }
                }
            } else {
                switch (GroupMethod) {
                case CDockGroupMenu::GroupToSubmenu:{
                    QAction *subaction = new QAction(groupName);
                    auto submenu = new QMenu();
                    subaction->setMenu(submenu);
                    fillMenu(submenu, actionList, actionMap);
                    resMenu->addAction(subaction);
                }break;
                case CDockGroupMenu::GroupToRoot:
                    fillMenu(resMenu, actionList, actionMap);
                    if (!actionList.empty()) {
                        resMenu->addSeparator();
                    }
                    break;
                default:
                    break;
                }
            }
        }
        return resMenu;
    }

    std::vector<std::pair<QString, std::vector<QString>>> List;
};


CDockGroupMenu::CDockGroupMenu(CDockManager *manager)
    : QObject(manager), d(new DockGroupMenuPrivate(this, manager))
{

}

void CDockGroupMenu::addGroup(const QString &name, int index)
{
    if (index > -1 && d->List.size() > index) {
        d->List.insert(d->List.begin() + index, {name, {}});
    } else {
        d->List.push_back({name, {}});
    }
}

void CDockGroupMenu::addWidget(const QString &groupName, CDockWidget *widget, int index)
{
    for (auto &g : d->List) {
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

void CDockGroupMenu::clear()
{
    d->List.clear();
}

void CDockGroupMenu::removeWidget(CDockWidget *widget)
{
    for (auto &g : d->List) {
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
    for (auto &ga : d->List) {
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

QMenu *CDockGroupMenu::getMenu(CDockAreaWidget *area)
{
    if (!d->DockManager) {
        return {};
    }

    std::map<QString, std::map<QString, QAction *>> groupActions;

    const auto &widgetsList = d->DockManager->dockWidgetsMap();
    // adding actions for all widgets
    // if widget tabbed - open it
    for (const auto &w : widgetsList) {
        QAction *action = nullptr;
        action = new QAction(w->windowTitle());
        connect(action, &QAction::triggered, this, [this, area, w]() {
            if (w->dockAreaWidget() != area) {
                d->DockManager->moveDockWidgetTabToArea(w, area);
            }
            w->toggleView(true);
        });
        const auto groupName = w->getGroupName();
        groupActions[groupName].insert({action->text(), action});
    }
    return d->createMenu(groupActions);
}

void CDockGroupMenu::setGroupMethod(CDockGroupMenu::GroupFlag method)
{
    d->GroupMethod = method;
}

}
