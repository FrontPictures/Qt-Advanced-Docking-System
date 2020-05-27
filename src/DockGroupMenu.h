#ifndef DOCKGROUPMENU_H
#define DOCKGROUPMENU_H

#include <QObject>
#include <QAction>

namespace ads
{
struct DockGroupMenuPrivate;
class CDockWidget;
class CDockAreaWidget;
class CDockManager;

/**
 * Group menu used for adding widget as tab to already existed tabs
 * If widget already in requested tab are it doesn't showed in menu
 * If widget already in requested tab but closed it showes in menu but action will just toggle it visibility
 */

class CDockGroupMenu : public QObject
{
    Q_OBJECT
public:
    explicit CDockGroupMenu(CDockManager *manager);

    /**
     * Insert sub menu with given name to group menu by given index
     * if index is out of range appends to end
     */
    void addGroup(const QString &name, int index = -1);

    /**
     * Insert widget's action to sub menu by given index
     * if index is out of range appends to end
     */
    void addWidget(const QString &groupName, CDockWidget *widget, int index = -1);

    /**
     * Remove widget
     */
    void removeWidget(CDockWidget *widget);

    /**
     * Rename action
     */
    void renameAction(const QString &groupName, const QString &oldName, const QString &newName);

    /**
     * Create action of group menu accoding to given area
     */
    QList<QAction *> getActions(CDockAreaWidget *area);

private:
    DockGroupMenuPrivate *d = nullptr;


};
}

#endif // DOCKGROUPMENU_H
