#include "clienticons.h"

#include <definitions/stanzahandlerorders.h>

#define RDR_CLIENTICONS 460
#define PROPERTY_CLIENT "client"
#define SHC_PRESENCE  "/presence/c[@xmlns="NS_CAPS"]"

static const QList<int> RosterKinds = QList<int>() << RIK_CONTACT << RIK_CONTACTS_ROOT;

ClientIcons::ClientIcons()
{
	FMainWindowPlugin = NULL;
	FPresenceManager = NULL;
	FXmppStreamManager = NULL;
	FStanzaProcessor = NULL;
	FOptionsManager = NULL;
	FRosterManager = NULL;
	FRostersModel = NULL;
	FRostersViewPlugin = NULL;
	FClientIconsLabelId = 0;

#ifdef DEBUG_RESOURCES_DIR
	FileStorage::setResourcesDirs(FileStorage::resourcesDirs() << DEBUG_RESOURCES_DIR);
#endif
}

ClientIcons::~ClientIcons()
{

}

void ClientIcons::pluginInfo(IPluginInfo *APluginInfo)
{
	APluginInfo->name = tr("Client Icons");
	APluginInfo->description = tr("Displays a client icon in the roster");
	APluginInfo->version = "0.4";
	APluginInfo->author = "Alexey Ivanov aka krab";
	APluginInfo->homePage = "http://code.google.com/p/vacuum-plugins";
	APluginInfo->dependences.append(XMPPSTREAMS_UUID);
	APluginInfo->dependences.append(PRESENCE_UUID);
}

bool ClientIcons::initConnections(IPluginManager *APluginManager, int &AInitOrder)
{
	AInitOrder = 30;

	IPlugin *plugin = APluginManager->pluginInterface("IMainWindowPlugin").value(0);
	if(plugin)
	{
		FMainWindowPlugin = qobject_cast<IMainWindowPlugin *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("IStanzaProcessor").value(0,NULL);
	if (plugin)
		FStanzaProcessor = qobject_cast<IStanzaProcessor *>(plugin->instance());

	plugin = APluginManager->pluginInterface("IXmppStreamManager").value(0, NULL);
	if(plugin)
	{
		FXmppStreamManager = qobject_cast<IXmppStreamManager *>(plugin->instance());
		if(FXmppStreamManager)
		{
			connect(FXmppStreamManager->instance(),SIGNAL(streamOpened(IXmppStream *)),SLOT(onStreamOpened(IXmppStream *)));
			connect(FXmppStreamManager->instance(),SIGNAL(streamClosed(IXmppStream *)),SLOT(onStreamClosed(IXmppStream *)));
		}
	}

	plugin = APluginManager->pluginInterface("IPresenceManager").value(0, NULL);
	if(plugin)
	{
		FPresenceManager = qobject_cast<IPresenceManager *>(plugin->instance());
		if(FPresenceManager)
		{
			connect(FPresenceManager->instance(),SIGNAL(contactStateChanged(const Jid &, const Jid &, bool)),
					SLOT(onContactStateChanged(const Jid &, const Jid &, bool)));
		}
	}

	plugin = APluginManager->pluginInterface("IRosterManager").value(0,NULL);
	if (plugin)
	{
		FRosterManager = qobject_cast<IRosterManager *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("IRostersModel").value(0, NULL);
	if(plugin)
	{
		FRostersModel = qobject_cast<IRostersModel *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("IRostersViewPlugin").value(0,NULL);
	if (plugin)
	{
		FRostersViewPlugin = qobject_cast<IRostersViewPlugin *>(plugin->instance());
		if (FRostersViewPlugin)
		{
			connect(FRostersViewPlugin->rostersView()->instance(),SIGNAL(indexToolTips(IRosterIndex *, quint32, QMap<int,QString> &)),
				SLOT(onRostersViewIndexToolTips(IRosterIndex *, quint32, QMap<int,QString> &)));
		}
	}

	plugin = APluginManager->pluginInterface("IOptionsManager").value(0, NULL);
	if(plugin)
	{
		FOptionsManager = qobject_cast<IOptionsManager *>(plugin->instance());
		if (FOptionsManager)
		{
			connect(Options::instance(),SIGNAL(optionsOpened()),SLOT(onOptionsOpened()));
			connect(Options::instance(),SIGNAL(optionsChanged(OptionsNode)),SLOT(onOptionsChanged(OptionsNode)));
		}
	}

	return FMainWindowPlugin != NULL && FRosterManager != NULL && FPresenceManager != NULL && FXmppStreamManager != NULL;
}

bool ClientIcons::initObjects()
{
	IconStorage *storage = IconStorage::staticStorage(RSR_STORAGE_CLIENTICONS);

	foreach (QString key, storage->fileFirstKeys())
	{
		Client client = {storage->fileProperty(key, PROPERTY_CLIENT), storage->getIcon(key)};
		FClients.insert(key, client);
	}

	if(FRostersModel)
	{
		FRostersModel->insertRosterDataHolder(RDHO_CLIENTICONS,this);
	}

	if(FRostersViewPlugin)
	{
		AdvancedDelegateItem label(RLID_CLIENTICONS);
		label.d->kind = AdvancedDelegateItem::CustomData;
		label.d->data = RDR_CLIENTICONS;
		FClientIconsLabelId = FRostersViewPlugin->rostersView()->registerLabel(label);

		FRostersViewPlugin->rostersView()->insertLabelHolder(RLHO_CLIENTICONS,this);
	}

	if (FOptionsManager)
	{
		FOptionsManager->insertOptionsDialogHolder(this);
	}

	return true;
}

bool ClientIcons::initSettings()
{
	Options::setDefaultValue(OPV_ROSTER_CLIENT_ICON_SHOW,true);

	return true;
}

QMultiMap<int, IOptionsDialogWidget *> ClientIcons::optionsDialogWidgets(const QString &ANodeId, QWidget *AParent)
{
	QMultiMap<int, IOptionsDialogWidget *> widgets;
	if (FOptionsManager && ANodeId==OPN_ROSTERVIEW)
	{
		widgets.insertMulti(OWO_ROSTER_CLIENTICONS,FOptionsManager->newOptionsDialogWidget(Options::node(OPV_ROSTER_CLIENT_ICON_SHOW),tr("Show contact application icon"),AParent));
	}
	return widgets;
}

bool ClientIcons::stanzaReadWrite(int AHandlerId, const Jid &AStreamJid, Stanza &AStanza, bool &AAccept)
{
	Q_UNUSED(AAccept);
	if (FSHIPresence.value(AStreamJid) == AHandlerId)
	{
		Jid contactJid = AStanza.from();
		QString node = AStanza.firstElement("c",NS_CAPS).attribute("node");
		if (FContacts.value(contactJid) != node)
		{
			if (!node.isEmpty() && AStanza.type()!="error")
				FContacts.insert(contactJid, node);
			else
				FContacts.remove(contactJid);
			updateDataHolder(AStreamJid, contactJid);
		}
	}
	return false;
}

QList<int> ClientIcons::rosterDataRoles(int AOrder) const
{
	if (AOrder == RDHO_CLIENTICONS)
		return QList<int>() << RDR_CLIENTICONS;
	return QList<int>();
}

QVariant ClientIcons::rosterData(int AOrder, const IRosterIndex *AIndex, int ARole) const
{
	if (AOrder == RDHO_CLIENTICONS)
	{
		switch (AIndex->kind())
		{
		case RIK_CONTACT:
			{
				if (ARole == RDR_CLIENTICONS)
				{
					return QIcon(contactIcon(AIndex->data(RDR_PREP_FULL_JID).toString()));
				}
			}
			break;
		}
	}
	return QVariant();
}

bool ClientIcons::setRosterData(int AOrder, const QVariant &AValue, IRosterIndex *AIndex, int ARole)
{
	Q_UNUSED(AOrder);
	Q_UNUSED(AIndex);
	Q_UNUSED(ARole);
	Q_UNUSED(AValue);
	return false;
}

QList<quint32> ClientIcons::rosterLabels(int AOrder, const IRosterIndex *AIndex) const
{
	QList<quint32> labels;
	if (AOrder==RLHO_CLIENTICONS && FClientIconsVisible && !AIndex->data(RDR_CLIENTICONS).isNull())
		labels.append(FClientIconsLabelId);
	return labels;
}

AdvancedDelegateItem ClientIcons::rosterLabel(int AOrder, quint32 ALabelId, const IRosterIndex *AIndex) const
{
	Q_UNUSED(AOrder);
	Q_UNUSED(AIndex);
	return FRostersViewPlugin->rostersView()->registeredLabel(ALabelId);
}


void ClientIcons::onRostersViewIndexToolTips(IRosterIndex *AIndex, quint32 ALabelId, QMap<int, QString> &AToolTips)
{
	if ((ALabelId==AdvancedDelegateItem::DisplayId && RosterKinds.contains(AIndex->kind())))
	{
		QStringList resources = AIndex->data(RDR_RESOURCES).toStringList();
		if (!resources.isEmpty())
		{
			for(int resIndex=0; resIndex<10 && resIndex<resources.count(); resIndex++)
			{
				int orderShift = resIndex*100;
				Jid contactJid = resources.at(resIndex);
				if (!contactClient(contactJid).isEmpty())
					AToolTips.insert(RTTO_CLIENTICONS+orderShift, QString("<b>%1</b> %2</div>").arg(tr("Client:")).arg(contactClient(contactJid)));
			}
		}
	}
}

void ClientIcons::updateDataHolder(const Jid &streamJid, const Jid &clientJid)
{
	if(FRostersModel)
	{
		QMultiMap<int, QVariant> findData;
		if (!streamJid.isEmpty())
			findData.insert(RDR_STREAM_JID,streamJid.pFull());
		if (!clientJid.isEmpty())
			findData.insert(RDR_PREP_BARE_JID,clientJid.pBare());
		findData.insert(RDR_KIND,RIK_CONTACT);

		foreach (IRosterIndex *index, FRostersModel->rootIndex()->findChilds(findData,true))
			emit rosterDataChanged(index,RDR_CLIENTICONS);
	}
}

void ClientIcons::onStreamOpened(IXmppStream *AXmppStream)
{
	if (FStanzaProcessor)
	{
		IStanzaHandle shandle;
		shandle.handler = this;
		shandle.streamJid = AXmppStream->streamJid();
		shandle.order = SHO_DEFAULT;
		shandle.direction = IStanzaHandle::DirectionIn;
		shandle.conditions.append(SHC_PRESENCE);
		FSHIPresence.insert(shandle.streamJid,FStanzaProcessor->insertStanzaHandle(shandle));
	}
}

void ClientIcons::onStreamClosed(IXmppStream *AXmppStream)
{
	if (FStanzaProcessor)
	{
		FStanzaProcessor->removeStanzaHandle(FSHIPresence.take(AXmppStream->streamJid()));
	}
}

void ClientIcons::onContactStateChanged(const Jid &streamJid, const Jid &contactJid, bool AStateOnline)
{
	if (!AStateOnline && FContacts.contains(contactJid))
	{
		FContacts.remove(contactJid);
		updateDataHolder(streamJid, contactJid);
	}
}

void ClientIcons::onOptionsOpened()
{
	onOptionsChanged(Options::node(OPV_ROSTER_CLIENT_ICON_SHOW));
}

void ClientIcons::onOptionsChanged(const OptionsNode &ANode)
{
	if (ANode.path() == OPV_ROSTER_CLIENT_ICON_SHOW)
	{
		FClientIconsVisible = ANode.value().toBool();
		emit rosterLabelChanged(FClientIconsLabelId,NULL);
	}
}

QIcon ClientIcons::iconByKey(const QString &key) const
{
	return FClients.value(key).icon;
}

QString ClientIcons::clientByKey(const QString &key) const
{
	return FClients.value(key).name;
}

QString ClientIcons::contactClient(const Jid &contactJid) const
{
	return FClients.value(FContacts.value(contactJid)).name;
}

QIcon ClientIcons::contactIcon(const Jid &contactJid) const
{
	return FClients.value(FContacts.value(contactJid)).icon;
}


Q_EXPORT_PLUGIN2(plg_clienticons, ClientIcons)
