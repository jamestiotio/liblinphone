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

#include "conference-info.h"

#include "chat/ics/ics.h"
#include "factory/factory.h"
#include "linphone/api/c-address.h"
#include "linphone/types.h"
#include "participant-info.h"
#include "private.h"

#include "c-wrapper/c-wrapper.h"

// =============================================================================

using namespace std;

LINPHONE_BEGIN_NAMESPACE

ConferenceInfo::ConferenceInfo() {
}

const ConferenceInfo::organizer_t &ConferenceInfo::getOrganizer() const {
	return mOrganizer;
}

const std::shared_ptr<Address> &ConferenceInfo::getOrganizerAddress() const {
	const auto &organizer = getOrganizer();
	mOrganizerAddress = organizer ? organizer->getAddress() : nullptr;
	return mOrganizerAddress;
}

void ConferenceInfo::setOrganizer(const std::shared_ptr<const ParticipantInfo> &organizer) {
	mOrganizer = organizer->clone()->toSharedPtr();
}

void ConferenceInfo::setOrganizer(const std::shared_ptr<const Address> &organizer) {
	auto organizerInfo = Factory::get()->createParticipantInfo(Address::create(organizer->getUri()));
	for (const auto &[name, value] : organizer->getParams()) {
		organizerInfo->addParameter(name, value);
	}
	setOrganizer(organizerInfo);
}

const ConferenceInfo::participant_list_t &ConferenceInfo::getParticipants() const {
	return mParticipants;
}

const bctbx_list_t *ConferenceInfo::getParticipantsCList() const {
	mParticipantList.mList = mParticipants;
	return mParticipantList.getCList();
}

const std::list<std::shared_ptr<Address>> &ConferenceInfo::getParticipantAddressList() const {
	updateParticipantAddresses();
	return mParticipantAddresses.mList;
}

const bctbx_list_t *ConferenceInfo::getParticipantAddressCList() const {
	updateParticipantAddresses();
	return mParticipantAddresses.getCList();
}

void ConferenceInfo::updateParticipantAddresses() const {
	std::list<std::shared_ptr<Address>> addressList;
	for (const auto &participantInfo : mParticipants) {
		addressList.push_back(participantInfo->getAddress());
	}
	mParticipantAddresses.mList = addressList;
}

void ConferenceInfo::setParticipants(const std::list<std::shared_ptr<Address>> &participants) {
	for (const auto &info : participants) {
		addParticipant(info);
	}
}

void ConferenceInfo::setParticipants(const ConferenceInfo::participant_list_t &participants) {
	for (const auto &info : participants) {
		addParticipant(info);
	}
}

void ConferenceInfo::addParticipant(const std::shared_ptr<const ParticipantInfo> &participantInfo) {
	const auto &address = participantInfo->getAddress();
	if (hasParticipant(address)) {
		mParticipants.push_back(participantInfo->clone()->toSharedPtr());
	} else {
		lInfo() << "Participant with address " << *address << " is already in the list of conference info " << this
		        << " (address " << (getUri() ? getUri()->toString() : std::string("<unknown address>")) << ")";
	}
}

void ConferenceInfo::addParticipant(const std::shared_ptr<const Address> &participant) {
	auto participantInfo = Factory::get()->createParticipantInfo(participant->clone()->toSharedPtr());
	addParticipant(participantInfo);
}

void ConferenceInfo::removeParticipant(const std::shared_ptr<const ParticipantInfo> &participantInfo) {
	removeParticipant(participantInfo->getAddress());
}

void ConferenceInfo::removeParticipant(const std::shared_ptr<const Address> &participant) {
	auto it = findParticipantIt(participant);
	if (it == mParticipants.cend()) {
		lInfo() << "Unable to remove participant with address " << *participant << " in conference info " << this
		        << " (address " << (getUri() ? getUri()->toString() : std::string("<unknown address>")) << ")";
	} else {
		mParticipants.erase(it);
	}
}

void ConferenceInfo::updateParticipant(const std::shared_ptr<const ParticipantInfo> &participantInfo) {
	removeParticipant(participantInfo);
	addParticipant(participantInfo);
}

ConferenceInfo::participant_list_t::const_iterator
ConferenceInfo::findParticipantIt(const std::shared_ptr<const Address> &address) const {
	return std::find_if(mParticipants.begin(), mParticipants.end(),
	                    [&address](const auto &p) { return (address->weakEqual(*p->getAddress())); });
}

bool ConferenceInfo::hasParticipant(const std::shared_ptr<const Address> &address) const {
	return (findParticipantIt(address) == mParticipants.end());
}

const std::shared_ptr<ParticipantInfo>
ConferenceInfo::findParticipant(const std::shared_ptr<const Address> &address) const {
	auto it = findParticipantIt(address);
	if (it != mParticipants.end()) {
		return *it;
	};
	lInfo() << "Unable to find participant with address " << *address << " in conference info " << this << " (address "
	        << (getUri() ? getUri()->toString() : std::string("<unknown address>")) << ")";
	return nullptr;
}

bool ConferenceInfo::isValidUri() const {
	return (mUri != nullptr) && mUri->isValid();
}

const std::shared_ptr<Address> &ConferenceInfo::getUri() const {
	return mUri;
}

void ConferenceInfo::setUri(const std::shared_ptr<const Address> uri) {
	mUri = Address::create(uri->getUri());
}

time_t ConferenceInfo::getDateTime() const {
	return mDateTime;
}

void ConferenceInfo::setDateTime(time_t dateTime) {
	mDateTime = dateTime;
}

unsigned int ConferenceInfo::getDuration() const {
	return mDuration;
}

void ConferenceInfo::setDuration(unsigned int duration) {
	mDuration = duration;
}

const std::string &ConferenceInfo::getSubject() const {
	return mSubject;
}

const std::string ConferenceInfo::getUtf8Subject() const {
	return Utils::localeToUtf8(mSubject);
}

void ConferenceInfo::setSubject(const std::string &subject) {
	mSubject = Utils::trim(subject);
}

void ConferenceInfo::setUtf8Subject(const std::string &subject) {
	mSubject = Utils::trim(Utils::utf8ToLocale(subject));
}

unsigned int ConferenceInfo::getIcsSequence() const {
	return mIcsSequence;
}

void ConferenceInfo::setIcsSequence(unsigned int icsSequence) {
	mIcsSequence = icsSequence;
}

const std::string ConferenceInfo::getUtf8IcsUid() const {
	return Utils::localeToUtf8(mIcsUid);
}

const std::string &ConferenceInfo::getIcsUid() const {
	return mIcsUid;
}

void ConferenceInfo::setUtf8IcsUid(const std::string &uid) {
	mIcsUid = Utils::trim(Utils::utf8ToLocale(uid));
}

void ConferenceInfo::setIcsUid(const std::string &uid) {
	mIcsUid = Utils::trim(uid);
}

const string ConferenceInfo::getUtf8Description() const {
	return Utils::localeToUtf8(mDescription);
}

const string &ConferenceInfo::getDescription() const {
	return mDescription;
}

void ConferenceInfo::setDescription(const string &description) {
	mDescription = Utils::trim(description);
}

void ConferenceInfo::setUtf8Description(const string &description) {
	mDescription = Utils::trim(Utils::utf8ToLocale(description));
}

ConferenceParamsInterface::SecurityLevel ConferenceInfo::getSecurityLevel() const {
	return mSecurityLevel;
}

void ConferenceInfo::setSecurityLevel(ConferenceParamsInterface::SecurityLevel securityLevel) {
	mSecurityLevel = securityLevel;
}

const ConferenceInfo::State &ConferenceInfo::getState() const {
	return mState;
}

void ConferenceInfo::setState(const ConferenceInfo::State &state) {
	if (mState != state) {
		lInfo() << "[Conference Info] [" << this << "] moving from state " << mState << " to state " << state;
		mState = state;
	}
}

void ConferenceInfo::updateFrom(const std::shared_ptr<ConferenceInfo> &info) {
	setUri(info->getUri());
	setIcsUid(info->getIcsUid());
	setIcsSequence(info->getIcsSequence() + 1);

	const auto &participants = info->getParticipants();
	for (auto &participant : mParticipants) {
		const auto &pAddress = participant->getAddress();
		const auto &otherParticipantIt =
		    std::find_if(participants.cbegin(), participants.cend(),
		                 [&pAddress](const auto &p) { return (pAddress->weakEqual(*p->getAddress())); });

		if (otherParticipantIt != participants.cend()) {
			const auto &otherParticipant = (*otherParticipantIt);
			// Copy sequence number in order to keep it inceasing.
			// IF this is not done, the sequence number may be arbitrarly changed and clients could get out of sync
			participant->setSequenceNumber(otherParticipant->getSequenceNumber());
		}
	}
}

#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif // _MSC_VER
const string ConferenceInfo::toIcsString(bool cancel, int sequence) const {
#ifdef HAVE_ADVANCED_IM
	Ics::Icalendar cal;

	Ics::Icalendar::Method method = Ics::Icalendar::Method::Request;
	if (cancel) {
		method = Ics::Icalendar::Method::Cancel;
	} else {
		switch (getState()) {
			case ConferenceInfo::State::New:
			case ConferenceInfo::State::Updated:
				method = Ics::Icalendar::Method::Request;
				break;
			case ConferenceInfo::State::Cancelled:
				method = Ics::Icalendar::Method::Cancel;
				break;
		}
	}
	cal.setMethod(method);

	auto event = make_shared<Ics::Event>();
	const auto &organizerAddress = getOrganizerAddress();
	if (organizerAddress && organizerAddress->isValid()) {
		const auto uri = organizerAddress->asStringUriOnly();
		event->setOrganizer(uri, mOrganizer->getAllParameters());
	}

	event->setSummary(mSubject);
	event->setDescription(mDescription);

	if (mUri && mUri->isValid()) {
		const auto uri = mUri->asStringUriOnly();
		event->setXConfUri(uri);
	}
	for (const auto &participantInfo : mParticipants) {
		const auto &address = participantInfo->getAddress();
		if (address->isValid()) {
			const auto uri = address->asStringUriOnly();
			event->addAttendee(uri, participantInfo->getAllParameters());
		}
	}

	event->setDateTimeStart(Utils::getTimeTAsTm(mDateTime));

	tm duration = {0};
	duration.tm_hour = mDuration / 60;
	duration.tm_min = mDuration % 60;
	event->setDuration(duration);

	event->setSequence((sequence >= 0) ? static_cast<unsigned int>(sequence) : mIcsSequence);

	if (!mIcsUid.empty()) {
		event->setUid(mIcsUid);
	}

	cal.addEvent(event);

	if (mCreationTime != (time_t)-1) {
		cal.setCreationTime(mCreationTime);
	}
	const auto icsString = cal.asString();

	if (mIcsUid.empty()) {
		mIcsUid = event->getUid();
	}
	if (mIcsSequence == 0) {
		mIcsSequence = event->getSequence();
	}

	return icsString;
#else
	lWarning() << "No ICS support, ADVANCED_IM is disabled.";
	return string();
#endif
}
#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif // _MSC_VER

void ConferenceInfo::setCreationTime(time_t time) {
	mCreationTime = time;
}

std::ostream &operator<<(std::ostream &lhs, ConferenceInfo::State s) {
	switch (s) {
		case ConferenceInfo::State::New:
			return lhs << "New";
		case ConferenceInfo::State::Updated:
			return lhs << "Updated";
		case ConferenceInfo::State::Cancelled:
			return lhs << "Cancelled";
	}
	return lhs;
}

LINPHONE_END_NAMESPACE
