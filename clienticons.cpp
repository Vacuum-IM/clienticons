#include "clienticons.h"

#define RDR_CLIENTICONS 460

#define PROPERTY_CLIENT "client"

#define SHC_PRESENCE              "/presence/c[@xmlns=http://jabber.org/protocol/caps]"

static const QList<int> RosterKinds = QList<int>() << RIK_CONTACT << RIK_CONTACTS_ROOT;

ClientIcons::ClientIcons()
{
	FMainWindowPlugin = NULL;
	FPresencePlugin = NULL;
	FXmppStreams = NULL;
	FStanzaProcessor = NULL;
	FOptionsManager = NULL;
	FRoster = NULL;
	FRosterPlugin = NULL;
	FRostersModel = NULL;
	FRostersViewPlugin = NULL;

	FClientIconLabelId = 0;

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
	APluginInfo->description = tr("Displays a client icon in the roster that uses the contact");
	APluginInfo->version = "0.1";
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

	plugin = APluginManager->pluginInterface("IXmppStreams").value(0, NULL);
	if(plugin)
	{
		FXmppStreams = qobject_cast<IXmppStreams *>(plugin->instance());
		if(FXmppStreams)
		{
			connect(FXmppStreams->instance(),SIGNAL(opened(IXmppStream *)),SLOT(onStreamOpened(IXmppStream *)));
			connect(FXmppStreams->instance(),SIGNAL(closed(IXmppStream *)),SLOT(onStreamClosed(IXmppStream *)));
		}
	}

	plugin = APluginManager->pluginInterface("IPresencePlugin").value(0, NULL);
	if(plugin)
	{
		FPresencePlugin = qobject_cast<IPresencePlugin *>(plugin->instance());
		if(FPresencePlugin)
		{
			connect(FPresencePlugin->instance(),SIGNAL(contactStateChanged(const Jid &, const Jid &, bool)),
					SLOT(onContactStateChanged(const Jid &, const Jid &, bool)));
		}
	}

	plugin = APluginManager->pluginInterface("IRoster").value(0, NULL);
	if(plugin)
	{
		FRoster = qobject_cast<IRoster *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("IRosterPlugin").value(0,NULL);
	if (plugin)
	{
		FRosterPlugin = qobject_cast<IRosterPlugin *>(plugin->instance());
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
	}

	return FMainWindowPlugin != NULL && FRosterPlugin !=NULL;
}

bool ClientIcons::initObjects()
{
	IconStorage *storage = IconStorage::staticStorage(RSR_STORAGE_CLIENTICONS);

	foreach (QString key, storage->fileKeys())
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
		FClientIconLabelId = FRostersViewPlugin->rostersView()->registerLabel(label);

		FRostersViewPlugin->rostersView()->insertLabelHolder(RLHO_CLIENTICONS,this);
	}

	return true;
}

bool ClientIcons::initSettings()
{
	Options::setDefaultValue(OPV_ROSTER_CLIENT_ICON_SHOW,true);

	if (FOptionsManager)
	{
		FOptionsManager->insertOptionsHolder(this);
	}
	return true;
}

QMultiMap<int, IOptionsWidget *> ClientIcons::optionsWidgets(const QString &ANodeId, QWidget *AParent)
{
	QMultiMap<int, IOptionsWidget *> widgets;
	if (FOptionsManager && ANodeId == OPN_ROSTER)
	{
		widgets.insertMulti(OWO_ROSTER_CLIENT_ICONS, FOptionsManager->optionsNodeWidget(Options::node(OPV_ROSTER_CLIENT_ICON_SHOW),tr("Show client icons"),AParent));
	}
	return widgets;
}

bool ClientIcons::stanzaReadWrite(int AHandlerId, const Jid &AStreamJid, Stanza &AStanza, bool &AAccept)
{
	if (FSHIPresence.value(AStreamJid) == AHandlerId)
	{
		AAccept = true;
		Jid contactJid = AStanza.from();
		QDomElement capsElem = AStanza.firstElement("c",NS_CAPS);
		FContacts.insert(contactJid, capsElem.attribute("node"));
		updateDataHolder(AStreamJid, contactJid);
		return AAccept;
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
		case RIK_STREAM_ROOT:
		case RIK_CONTACT:
		case RIK_CONTACTS_ROOT:
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
	if (AOrder==RLHO_CLIENTICONS && FShowClientIcons && !AIndex->data(RDR_CLIENTICONS).isNull())
		labels.append(FClientIconLabelId);
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
	if ((ALabelId==AdvancedDelegateItem::DisplayId && RosterKinds.contains(AIndex->kind())) || ALabelId == FClientIconLabelId)
	{
		Jid contactJid = AIndex->data(RDR_FULL_JID).toString();
		if (!contactClient(contactJid).isEmpty())
		{
			QString tooltip = QString("<b>%1</b> %2</div>").arg(tr("Client:")).arg(contactClient(contactJid));
			AToolTips.insert(RTTO_CLIENTICONS, tooltip);
		}
	}
}

void ClientIcons::updateDataHolder(const Jid &streamJid, const Jid &clientJid)
{
	if(FRostersModel)
	{
		QMultiMap<int, QVariant> findData;
		if (!clientJid.isEmpty())
			findData.insert(RDR_PREP_FULL_JID,clientJid.pFull());
		findData.insert(RDR_KIND,RIK_CONTACT);
		findData.insert(RDR_KIND,RIK_CONTACTS_ROOT);

		foreach (IRosterIndex *index, FRostersModel->streamRoot(streamJid)->findChilds(findData, true))
		{
			if(FContacts.contains(index->data(RDR_PREP_FULL_JID).toString()))
			{
				FRostersViewPlugin->rostersView()->insertLabel(FClientIconLabelId,index);
			}
			else
			{
				FRostersViewPlugin->rostersView()->removeLabel(FClientIconLabelId,index);
			}
			emit rosterDataChanged(index, RDR_CLIENTICONS);
		}
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
		FShowClientIcons = ANode.value().toBool();
		emit rosterLabelChanged(FClientIconLabelId,NULL);
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


Q_EXPORT_PLUGIN2(plg_pepmanager, ClientIcons)
