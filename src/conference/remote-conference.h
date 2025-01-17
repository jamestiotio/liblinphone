/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of Liblinphone
 * (see https://gitlab.linphone.org/BC/public/liblinphone).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _L_REMOTE_CONFERENCE_H_
#define _L_REMOTE_CONFERENCE_H_

#include "conference.h"
#include "core/core-accessor.h"

// =============================================================================

LINPHONE_BEGIN_NAMESPACE

class RemoteConferenceEventHandler;
class Core;

class LINPHONE_PUBLIC RemoteConference : public Conference, public ConferenceListenerInterface {
	friend class ClientGroupChatRoomPrivate;
	friend class ClientGroupChatRoom;

public:
	RemoteConference(const std::shared_ptr<Core> &core,
	                 const std::shared_ptr<Address> &myAddress,
	                 CallSessionListener *listener,
	                 const std::shared_ptr<ConferenceParams> params);
	virtual ~RemoteConference();

	virtual bool isIn() const override;

protected:
	std::shared_ptr<Participant> focus;
#ifdef HAVE_ADVANCED_IM
	std::shared_ptr<RemoteConferenceEventHandler> eventHandler;
#endif // HAVE_ADVANCED_IM

	/* ConferenceListener */
	void onConferenceCreated(const std::shared_ptr<Address> &addr) override;
	void onConferenceTerminated(const std::shared_ptr<Address> &addr) override;
	void onFirstNotifyReceived(const std::shared_ptr<Address> &addr) override;
	void onParticipantAdded(const std::shared_ptr<ConferenceParticipantEvent> &event,
	                        const std::shared_ptr<Participant> &participant) override;
	void onParticipantRemoved(const std::shared_ptr<ConferenceParticipantEvent> &event,
	                          const std::shared_ptr<Participant> &participant) override;
	void onParticipantSetAdmin(const std::shared_ptr<ConferenceParticipantEvent> &event,
	                           const std::shared_ptr<Participant> &participant) override;
	void onSubjectChanged(const std::shared_ptr<ConferenceSubjectEvent> &event) override;
	void onParticipantDeviceAdded(const std::shared_ptr<ConferenceParticipantDeviceEvent> &event,
	                              const std::shared_ptr<ParticipantDevice> &device) override;
	void onParticipantDeviceRemoved(const std::shared_ptr<ConferenceParticipantDeviceEvent> &event,
	                                const std::shared_ptr<ParticipantDevice> &device) override;
	void onFullStateReceived() override;

	void onEphemeralModeChanged(const std::shared_ptr<ConferenceEphemeralMessageEvent> &event) override;
	void onEphemeralMessageEnabled(const std::shared_ptr<ConferenceEphemeralMessageEvent> &event) override;
	void onEphemeralLifetimeChanged(const std::shared_ptr<ConferenceEphemeralMessageEvent> &event) override;
	virtual std::shared_ptr<Call> getCall() const override;

private:
	L_DISABLE_COPY(RemoteConference);
};

LINPHONE_END_NAMESPACE

#endif // ifndef _L_REMOTE_CONFERENCE_H_
