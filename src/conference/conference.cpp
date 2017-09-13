/*
 * conference.cpp
 * Copyright (C) 2017  Belledonne Communications SARL
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "participant-p.h"

#include "conference.h"

using namespace std;

LINPHONE_BEGIN_NAMESPACE

// =============================================================================

Conference::Conference (LinphoneCore *core, const Address &myAddress, CallListener *listener)
	: core(core), callListener(listener) {
	me = make_shared<Participant>(myAddress);
}

// -----------------------------------------------------------------------------

shared_ptr<Participant> Conference::getActiveParticipant () const {
	return activeParticipant;
}

// -----------------------------------------------------------------------------

shared_ptr<Participant> Conference::addParticipant (const Address &addr, const shared_ptr<CallSessionParams> params, bool hasMedia) {
	activeParticipant = make_shared<Participant>(addr);
	activeParticipant->getPrivate()->createSession(*this, params, hasMedia, this);
	return activeParticipant;
}

void Conference::addParticipants (const list<Address> &addresses, const shared_ptr<CallSessionParams> params, bool hasMedia) {
	// TODO
}

const string& Conference::getId () const {
	return id;
}

int Conference::getNbParticipants () const {
	// TODO
	return 1;
}

list<shared_ptr<Participant>> Conference::getParticipants () const {
	return participants;
}

void Conference::removeParticipant (const shared_ptr<Participant> participant) {
	// TODO
}

void Conference::removeParticipants (const list<shared_ptr<Participant>> participants) {
	// TODO
}

// -----------------------------------------------------------------------------

void Conference::onAckBeingSent (const CallSession &session, LinphoneHeaders *headers) {
	if (callListener)
		callListener->onAckBeingSent(headers);
}

void Conference::onAckReceived (const CallSession &session, LinphoneHeaders *headers) {
	if (callListener)
		callListener->onAckReceived(headers);
}

void Conference::onCallSessionAccepted (const CallSession &session) {
	if (callListener)
		callListener->onIncomingCallToBeAdded();
}

void Conference::onCallSessionSetReleased (const CallSession &session) {
	if (callListener)
		callListener->onCallSetReleased();
}

void Conference::onCallSessionSetTerminated (const CallSession &session) {
	if (callListener)
		callListener->onCallSetTerminated();
}

void Conference::onCallSessionStateChanged (const CallSession &session, LinphoneCallState state, const string &message) {
	if (callListener)
		callListener->onCallStateChanged(state, message);
}

void Conference::onIncomingCallSessionStarted (const CallSession &session) {
	if (callListener)
		callListener->onIncomingCallStarted();
}

void Conference::onEncryptionChanged (const CallSession &session, bool activated, const string &authToken) {
	if (callListener)
		callListener->onEncryptionChanged(activated, authToken);
}

void Conference::onStatsUpdated (const LinphoneCallStats *stats) {
	if (callListener)
		callListener->onStatsUpdated(stats);
}

void Conference::onResetCurrentSession (const CallSession &session) {
	if (callListener)
		callListener->onResetCurrentCall();
}

void Conference::onSetCurrentSession (const CallSession &session) {
	if (callListener)
		callListener->onSetCurrentCall();
}

void Conference::onFirstVideoFrameDecoded (const CallSession &session) {
	if (callListener)
		callListener->onFirstVideoFrameDecoded();
}

void Conference::onResetFirstVideoFrameDecoded (const CallSession &session) {
	if (callListener)
		callListener->onResetFirstVideoFrameDecoded();
}

LINPHONE_END_NAMESPACE
