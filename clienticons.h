#ifndef CLIENTICONS_H
#define CLIENTICONS_H

#include <QDebug>

#include "iclienticons.h"

#include <interfaces/imainwindow.h>
#include <interfaces/inotifications.h>
#include <interfaces/ioptionsmanager.h>
#include <interfaces/ipluginmanager.h>
#include <interfaces/ipresence.h>
#include <interfaces/iroster.h>
#include <interfaces/irostersmodel.h>
#include <interfaces/irostersview.h>
#include <interfaces/istanzaprocessor.h>
#include <interfaces/ixmppstreams.h>

#include <definitions/menuicons.h>
#include <definitions/namespaces.h>
#include <definitions/notificationdataroles.h>
#include <definitions/notificationtypeorders.h>
#include <definitions/notificationtypes.h>
#include <definitions/optionnodes.h>
#include <definitions/optionvalues.h>
#include <definitions/optionwidgetorders.h>
#include <definitions/resources.h>
#include <definitions/rosterdataholderorders.h>
#include <definitions/rosterindexkinds.h>
#include <definitions/rosterindexroles.h>
#include <definitions/rostertooltiporders.h>

#include <utils/action.h>
#include <utils/advanceditemdelegate.h>
#include <utils/filestorage.h>
#include <utils/menu.h>
#include <utils/options.h>

class ClientIcons :
	public QObject,
	public IPlugin,
	public IStanzaHandler,
	public IClientIcons,
	public IRosterDataHolder,
	public IRostersLabelHolder,
	public IOptionsHolder
{
	Q_OBJECT;
	Q_INTERFACES(IPlugin IStanzaHandler IRosterDataHolder IRostersLabelHolder IOptionsHolder IClientIcons IRosterDataHolder);

public:
	ClientIcons();
	~ClientIcons();
	//IPlugin
	virtual QObject *instance() { return this; }
	virtual QUuid pluginUuid() const { return CLIENTICONS_UUID; }
	virtual void pluginInfo(IPluginInfo *APluginInfo);
	virtual bool initConnections(IPluginManager *APluginManager, int &AInitOrder);
	virtual bool initObjects();
	virtual bool initSettings();
	virtual bool startPlugin() { return true; }
	//IOptionsHolder
	virtual QMultiMap<int, IOptionsWidget *> optionsWidgets(const QString &ANodeId, QWidget *AParent);
	//IStanzaHandler
	virtual bool stanzaReadWrite(int AHandlerId, const Jid &AStreamJid, Stanza &AStanza, bool &AAccept);
	//IRosterDataHolder
	virtual QList<int> rosterDataRoles(int AOrder) const;
	virtual QVariant rosterData(int AOrder, const IRosterIndex *AIndex, int ARole) const;
	virtual bool setRosterData(int AOrder, const QVariant &AValue, IRosterIndex *AIndex, int ARole);
	//IRostersLabelHolder
	virtual QList<quint32> rosterLabels(int AOrder, const IRosterIndex *AIndex) const;
	virtual AdvancedDelegateItem rosterLabel(int AOrder, quint32 ALabelId, const IRosterIndex *AIndex) const;
	//IClientIcons
	virtual QIcon iconByKey(const QString &key) const;
	virtual QString clientByKey(const QString &key) const;
	virtual QString contactClient(const Jid &contactJid) const;
	virtual QIcon contactIcon(const Jid &contactJid) const;

signals:
	//IRosterDataHolder
	void rosterDataChanged(IRosterIndex *AIndex = NULL, int ARole = 0);
	//IRostersLabelHolder
	void rosterLabelChanged(quint32 ALabelId, IRosterIndex *AIndex = NULL);

protected slots:
	//IRostersView
	void onRostersViewIndexToolTips(IRosterIndex *AIndex, quint32 ALabelId, QMap<int, QString> &AToolTips);
	//IXmppStreams
	void onStreamOpened(IXmppStream *AXmppStream);
	void onStreamClosed(IXmppStream *AXmppStream);
	//IPresencePlugin
	void onContactStateChanged(const Jid &streamJid, const Jid &contactJid, bool AStateOnline);
	//IOptionsHolder
	void onOptionsOpened();
	void onOptionsChanged(const OptionsNode &ANode);

protected:
	//IRosterDataHolder
	void updateDataHolder(const Jid &streamJid, const Jid &clientJid);

private:
	IMainWindowPlugin *FMainWindowPlugin;
	IPresencePlugin *FPresencePlugin;
	IStanzaProcessor *FStanzaProcessor;
	IXmppStreams *FXmppStreams;
	IOptionsManager *FOptionsManager;
	IRoster *FRoster;
	IRosterPlugin *FRosterPlugin;
	IRostersModel *FRostersModel;
	IRostersViewPlugin *FRostersViewPlugin;
	INotifications *FNotifications;

private:
	bool FShowClientIcons;
	quint32 FClientIconLabelId;

	QMap<Jid, int> FSHIPresence;

	QHash<QString, Client> FClients;
	QHash<Jid, QString> FContacts;
};

#endif // CLIENTICONS_H
