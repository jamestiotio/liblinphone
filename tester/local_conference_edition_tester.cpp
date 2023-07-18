/*
 * copyright (c) 2010-2023 belledonne communications sarl.
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

#include "liblinphone_tester.h"
#include "local_conference_tester_functions.h"
#include "shared_tester_functions.h"

namespace LinphoneTest {

static void edit_simple_conference_base(bool_t from_organizer,
                                        bool_t use_default_account,
                                        bool_t enable_bundle_mode,
                                        bool_t join,
                                        bool_t enable_encryption,
                                        bool_t server_restart,
                                        LinphoneConferenceSecurityLevel security_level) {
	Focus focus("chloe_rc");
	{ // to make sure focus is destroyed after clients.
		ClientConference marie("marie_rc", focus.getIdentity());
		ClientConference pauline("pauline_rc", focus.getIdentity());
		ClientConference laure("laure_tcp_rc", focus.getIdentity());
		ClientConference michelle("michelle_rc", focus.getIdentity());

		LinphoneCoreManager *manager_editing = (from_organizer) ? marie.getCMgr() : laure.getCMgr();
		linphone_core_enable_rtp_bundle(manager_editing->lc, enable_bundle_mode);
		int default_index =
		    linphone_config_get_int(linphone_core_get_config(manager_editing->lc), "sip", "default_proxy", 0);
		LinphoneAccountParams *params = linphone_account_params_new_with_config(manager_editing->lc, default_index);
		LinphoneAddress *alternative_address = linphone_address_new("sip:toto@sip.linphone.org");
		linphone_account_params_set_identity_address(params, alternative_address);
		LinphoneAccount *new_account = linphone_account_new(manager_editing->lc, params);
		linphone_core_add_account(manager_editing->lc, new_account);
		linphone_account_params_unref(params);
		linphone_account_unref(new_account);

		focus.registerAsParticipantDevice(marie);
		focus.registerAsParticipantDevice(pauline);
		focus.registerAsParticipantDevice(laure);
		focus.registerAsParticipantDevice(michelle);

		setup_conference_info_cbs(marie.getCMgr());
		linphone_core_set_file_transfer_server(marie.getLc(), file_transfer_url);

		for (auto mgr : {focus.getCMgr(), marie.getCMgr(), pauline.getCMgr(), laure.getCMgr()}) {
			LinphoneVideoActivationPolicy *pol =
			    linphone_factory_create_video_activation_policy(linphone_factory_get());
			linphone_video_activation_policy_set_automatically_accept(pol, TRUE);
			linphone_video_activation_policy_set_automatically_initiate(pol, TRUE);
			linphone_core_set_video_activation_policy(mgr->lc, pol);
			linphone_video_activation_policy_unref(pol);

			linphone_core_set_video_device(mgr->lc, liblinphone_tester_mire_id);
			linphone_core_enable_video_capture(mgr->lc, TRUE);
			linphone_core_enable_video_display(mgr->lc, TRUE);

			if ((mgr != focus.getCMgr()) && enable_encryption) {
				linphone_config_set_int(linphone_core_get_config(mgr->lc), "rtp", "accept_any_encryption", 1);
				linphone_core_set_media_encryption_mandatory(mgr->lc, TRUE);
				linphone_core_set_media_encryption(mgr->lc, LinphoneMediaEncryptionZRTP);
			}
		}

		bctbx_list_t *coresList = bctbx_list_append(NULL, focus.getLc());
		coresList = bctbx_list_append(coresList, marie.getLc());
		coresList = bctbx_list_append(coresList, pauline.getLc());
		coresList = bctbx_list_append(coresList, laure.getLc());
		coresList = bctbx_list_append(coresList, michelle.getLc());

		std::list<LinphoneCoreManager *> invitedParticipants{pauline.getCMgr(), laure.getCMgr()};

		time_t start_time = time(NULL) + 600; // Start in 10 minutes
		int duration = 60;                    // minutes
		time_t end_time = (duration <= 0) ? -1 : (start_time + duration * 60);
		const char *initialSubject = "Test characters: <S-F12><S-F11><S-F6> £$%§";
		const char *description = "Testing characters";

		LinphoneAddress *confAddr = create_conference_on_server(focus, marie, invitedParticipants, start_time, end_time,
		                                                        initialSubject, description, TRUE, security_level);
		BC_ASSERT_PTR_NOT_NULL(confAddr);

		// Chat room creation to send ICS
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneConferenceStateCreated, 2,
		                             liblinphone_tester_sip_timeout));

		const char *subject = "Test characters: <S-F12><S-F11><S-F6> £$%§ (+edits)";
		const char *description2 = "Testing characters (+edits)";

		stats manager_editing_stat = manager_editing->stat;
		LinphoneAccount *editing_account = NULL;
		if (use_default_account) {
			editing_account = linphone_core_get_default_account(manager_editing->lc);
		} else {
			editing_account = linphone_core_lookup_known_account(manager_editing->lc, alternative_address);
		}
		BC_ASSERT_PTR_NOT_NULL(editing_account);
		if (editing_account) {
			LinphoneAccountParams *account_params =
			    linphone_account_params_clone(linphone_account_get_params(editing_account));
			linphone_account_params_enable_rtp_bundle(account_params, enable_bundle_mode);
			linphone_account_set_params(editing_account, account_params);
			linphone_account_params_unref(account_params);
		}

		char *uid = NULL;
		unsigned int sequence = 0;
		LinphoneConferenceInfo *info = linphone_core_find_conference_information_from_uri(marie.getLc(), confAddr);
		if (BC_ASSERT_PTR_NOT_NULL(info)) {
			uid = ms_strdup(linphone_conference_info_get_ics_uid(info));
			BC_ASSERT_PTR_NOT_NULL(uid);
			sequence = linphone_conference_info_get_ics_sequence(info);
			linphone_conference_info_unref(info);
		}

		if (join) {
			std::list<LinphoneCoreManager *> participants{pauline.getCMgr()};
			std::list<LinphoneCoreManager *> conferenceMgrs{focus.getCMgr(), marie.getCMgr(), pauline.getCMgr()};
			std::list<LinphoneCoreManager *> members{marie.getCMgr(), pauline.getCMgr()};

			stats focus_stat = focus.getStats();
			for (auto mgr : members) {
				LinphoneCallParams *new_params = linphone_core_create_call_params(mgr->lc, nullptr);
				linphone_core_invite_address_with_params_2(mgr->lc, confAddr, new_params, NULL, nullptr);
				linphone_call_params_unref(new_params);
			}

			for (auto mgr : members) {
				BC_ASSERT_TRUE(wait_for_list(coresList, &mgr->stat.number_of_LinphoneCallOutgoingProgress, 1,
				                             liblinphone_tester_sip_timeout));
				int no_streams_running = 2;
				BC_ASSERT_TRUE(wait_for_list(coresList, &mgr->stat.number_of_LinphoneCallUpdating,
				                             (no_streams_running - 1), liblinphone_tester_sip_timeout));
				BC_ASSERT_TRUE(wait_for_list(coresList, &mgr->stat.number_of_LinphoneCallStreamsRunning,
				                             no_streams_running, liblinphone_tester_sip_timeout));
				// Update to add to conference.
				// If ICE is enabled, the addition to a conference may go through a resume of the call
				BC_ASSERT_TRUE(wait_for_list(coresList, &mgr->stat.number_of_LinphoneConferenceStateCreated,
				                             ((mgr == marie.getCMgr()) ? 3 : 2), liblinphone_tester_sip_timeout));
				BC_ASSERT_TRUE(
				    wait_for_list(coresList, &mgr->stat.number_of_LinphoneSubscriptionOutgoingProgress, 1, 5000));
				BC_ASSERT_TRUE(wait_for_list(coresList, &mgr->stat.number_of_LinphoneSubscriptionActive, 1, 5000));
				BC_ASSERT_TRUE(wait_for_list(coresList, &mgr->stat.number_of_NotifyFullStateReceived, 1,
				                             liblinphone_tester_sip_timeout));
			}

			BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneCallIncomingReceived,
			                             focus_stat.number_of_LinphoneCallIncomingReceived + 2,
			                             liblinphone_tester_sip_timeout));
			int focus_no_streams_running = 4;
			// Update to end ICE negotiations
			BC_ASSERT_TRUE(
			    wait_for_list(coresList, &focus.getStats().number_of_LinphoneCallUpdatedByRemote,
			                  focus_stat.number_of_LinphoneCallUpdatedByRemote + (focus_no_streams_running - 2),
			                  liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneCallStreamsRunning,
			                             focus_stat.number_of_LinphoneCallStreamsRunning + focus_no_streams_running,
			                             liblinphone_tester_sip_timeout));
			// Update to add to conference.
			// If ICE is enabled, the addition to a conference may go through a resume of the call
			BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneConferenceStateCreated,
			                             focus_stat.number_of_LinphoneConferenceStateCreated + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneSubscriptionIncomingReceived,
			                             focus_stat.number_of_LinphoneSubscriptionIncomingReceived + 2, 5000));
			BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneSubscriptionActive,
			                             focus_stat.number_of_LinphoneSubscriptionActive + 2, 5000));

			BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_participants_added,
			                             focus_stat.number_of_participants_added + 2, liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_participant_devices_added,
			                             focus_stat.number_of_participant_devices_added + 2,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_participant_devices_joined,
			                             focus_stat.number_of_participant_devices_joined + 2,
			                             liblinphone_tester_sip_timeout));

			wait_for_conference_streams({focus, marie, pauline, laure, michelle}, conferenceMgrs, focus.getCMgr(),
			                            members, confAddr, TRUE);

			LinphoneConference *fconference = linphone_core_search_conference_2(focus.getLc(), confAddr);
			BC_ASSERT_PTR_NOT_NULL(fconference);

			// wait bit more to detect side effect if any
			CoreManagerAssert({focus, marie, pauline, laure, michelle}).waitUntil(chrono::seconds(2), [] {
				return false;
			});

			for (auto mgr : conferenceMgrs) {
				LinphoneConference *pconference = linphone_core_search_conference_2(mgr->lc, confAddr);
				BC_ASSERT_PTR_NOT_NULL(pconference);
				if (pconference) {
					const LinphoneConferenceParams *conference_params =
					    linphone_conference_get_current_params(pconference);
					int no_participants = 0;
					if (start_time >= 0) {
						BC_ASSERT_EQUAL((long long)linphone_conference_params_get_start_time(conference_params),
						                (long long)start_time, long long, "%lld");
					}
					BC_ASSERT_EQUAL((long long)linphone_conference_params_get_end_time(conference_params),
					                (long long)end_time, long long, "%lld");

					bctbx_list_t *participant_device_list =
					    linphone_conference_get_participant_device_list(pconference);
					BC_ASSERT_EQUAL(bctbx_list_size(participant_device_list), 2, size_t, "%zu");
					for (bctbx_list_t *d_it = participant_device_list; d_it; d_it = bctbx_list_next(d_it)) {
						LinphoneParticipantDevice *d = (LinphoneParticipantDevice *)bctbx_list_get_data(d_it);
						BC_ASSERT_PTR_NOT_NULL(d);
						if (d) {
							BC_ASSERT_TRUE((!!linphone_participant_device_get_is_muted(d)) ==
							               (linphone_address_weak_equal(linphone_participant_device_get_address(d),
							                                            laure.getCMgr()->identity)));
							linphone_participant_device_set_user_data(d, mgr->lc);
							LinphoneParticipantDeviceCbs *cbs =
							    linphone_factory_create_participant_device_cbs(linphone_factory_get());
							linphone_participant_device_cbs_set_is_muted(cbs, on_muted_notified);
							linphone_participant_device_add_callbacks(d, cbs);
							linphone_participant_device_cbs_unref(cbs);
						}
					}
					bctbx_list_free_with_data(participant_device_list,
					                          (void (*)(void *))linphone_participant_device_unref);

					if (mgr == focus.getCMgr()) {
						no_participants = 2;
						BC_ASSERT_FALSE(linphone_conference_is_in(pconference));
					} else {
						no_participants = 1;
						BC_ASSERT_TRUE(linphone_conference_is_in(pconference));
						LinphoneCall *current_call = linphone_core_get_current_call(mgr->lc);
						BC_ASSERT_PTR_NOT_NULL(current_call);
						if (current_call) {
							BC_ASSERT_EQUAL((int)linphone_call_get_state(current_call),
							                (int)LinphoneCallStateStreamsRunning, int, "%0d");
						}

						LinphoneVideoActivationPolicy *pol = linphone_core_get_video_activation_policy(mgr->lc);
						bool_t enabled = !!linphone_video_activation_policy_get_automatically_initiate(pol);
						linphone_video_activation_policy_unref(pol);

						size_t no_streams_audio = 1;
						size_t no_streams_video = 3;
						size_t no_active_streams_video = no_streams_video;
						size_t no_streams_text = 0;

						LinphoneCall *pcall = linphone_core_get_call_by_remote_address2(mgr->lc, confAddr);
						BC_ASSERT_PTR_NOT_NULL(pcall);
						if (pcall) {
							_linphone_call_check_max_nb_streams(pcall, no_streams_audio, no_streams_video,
							                                    no_streams_text);
							_linphone_call_check_nb_active_streams(pcall, no_streams_audio, no_active_streams_video,
							                                       no_streams_text);
							const LinphoneCallParams *call_lparams = linphone_call_get_params(pcall);
							BC_ASSERT_EQUAL(linphone_call_params_video_enabled(call_lparams), enabled, int, "%0d");
							const LinphoneCallParams *call_rparams = linphone_call_get_remote_params(pcall);
							BC_ASSERT_EQUAL(linphone_call_params_video_enabled(call_rparams), enabled, int, "%0d");
							const LinphoneCallParams *call_cparams = linphone_call_get_current_params(pcall);
							BC_ASSERT_EQUAL(linphone_call_params_video_enabled(call_cparams), enabled, int, "%0d");
						}
						LinphoneCall *ccall = linphone_core_get_call_by_remote_address2(focus.getLc(), mgr->identity);
						BC_ASSERT_PTR_NOT_NULL(ccall);
						if (ccall) {
							_linphone_call_check_max_nb_streams(ccall, no_streams_audio, no_streams_video,
							                                    no_streams_text);
							_linphone_call_check_nb_active_streams(ccall, no_streams_audio, no_active_streams_video,
							                                       no_streams_text);
							const LinphoneCallParams *call_lparams = linphone_call_get_params(ccall);
							BC_ASSERT_EQUAL(linphone_call_params_video_enabled(call_lparams), enabled, int, "%0d");
							const LinphoneCallParams *call_rparams = linphone_call_get_remote_params(ccall);
							BC_ASSERT_EQUAL(linphone_call_params_video_enabled(call_rparams), enabled, int, "%0d");
							const LinphoneCallParams *call_cparams = linphone_call_get_current_params(ccall);
							BC_ASSERT_EQUAL(linphone_call_params_video_enabled(call_cparams), enabled, int, "%0d");
						}
					}
					BC_ASSERT_EQUAL(linphone_conference_get_participant_count(pconference), no_participants, int,
					                "%0d");
					BC_ASSERT_STRING_EQUAL(linphone_conference_get_subject(pconference), initialSubject);
					LinphoneParticipant *me = linphone_conference_get_me(pconference);
					BC_ASSERT_TRUE(linphone_participant_is_admin(me) ==
					               ((mgr == marie.getCMgr()) || (mgr == focus.getCMgr())));
					BC_ASSERT_TRUE(linphone_address_weak_equal(linphone_participant_get_address(me), mgr->identity));
					bctbx_list_t *participants = linphone_conference_get_participant_list(pconference);
					for (bctbx_list_t *itp = participants; itp; itp = bctbx_list_next(itp)) {
						LinphoneParticipant *p = (LinphoneParticipant *)bctbx_list_get_data(itp);
						BC_ASSERT_TRUE(linphone_participant_is_admin(p) ==
						               linphone_address_weak_equal(linphone_participant_get_address(p),
						                                           marie.getCMgr()->identity));
					}
					bctbx_list_free_with_data(participants, (void (*)(void *))linphone_participant_unref);

					if (mgr != focus.getCMgr()) {
						check_conference_ssrc(fconference, pconference);
					}
				}
			}
		}

		if (server_restart) {
			ms_message("%s is restarting", linphone_core_get_identity(focus.getLc()));
			coresList = bctbx_list_remove(coresList, focus.getLc());
			// Restart flexisip
			focus.reStart();

			LinphoneVideoActivationPolicy *pol =
			    linphone_factory_create_video_activation_policy(linphone_factory_get());
			linphone_video_activation_policy_set_automatically_accept(pol, TRUE);
			linphone_video_activation_policy_set_automatically_initiate(pol, TRUE);
			linphone_core_set_video_activation_policy(focus.getLc(), pol);
			linphone_video_activation_policy_unref(pol);

			linphone_core_enable_video_capture(focus.getLc(), TRUE);
			linphone_core_enable_video_display(focus.getLc(), TRUE);
			coresList = bctbx_list_append(coresList, focus.getLc());
		}

		bool_t add = TRUE;
		for (int attempt = 0; attempt < 3; attempt++) {
			char *conference_address_str = (confAddr) ? linphone_address_as_string(confAddr) : ms_strdup("<unknown>");
			ms_message("%s is trying to update conference %s - attempt %0d - %s %s",
			           linphone_core_get_identity(manager_editing->lc), conference_address_str, attempt,
			           (add) ? "adding" : "removing", linphone_core_get_identity(michelle.getLc()));
			ms_free(conference_address_str);

			stats focus_stat = focus.getStats();

			std::list<LinphoneCoreManager *> participants{pauline.getCMgr(), laure.getCMgr()};
			LinphoneConferenceInfo *conf_info =
			    linphone_core_find_conference_information_from_uri(manager_editing->lc, confAddr);
			BC_ASSERT_PTR_NOT_NULL(conf_info);
			if (conf_info) {
				linphone_conference_info_set_subject(conf_info, subject);
				linphone_conference_info_set_description(conf_info, description2);
				if (add) {
					linphone_conference_info_add_participant(conf_info, michelle.getCMgr()->identity);
					participants.push_back(michelle.getCMgr());
				} else {
					linphone_conference_info_remove_participant(conf_info, michelle.getCMgr()->identity);
				}

				const auto ics_participant_number = ((add) ? 3 : 2) + ((join) ? 1 : 0);
				bctbx_list_t *ics_participants = linphone_conference_info_get_participants(conf_info);
				BC_ASSERT_EQUAL(bctbx_list_size(ics_participants), ics_participant_number, size_t, "%zu");
				bctbx_list_free(ics_participants);

				std::list<stats> participant_stats;
				for (auto mgr :
				     {focus.getCMgr(), marie.getCMgr(), laure.getCMgr(), pauline.getCMgr(), michelle.getCMgr()}) {
					participant_stats.push_back(mgr->stat);
				}

				LinphoneConferenceScheduler *conference_scheduler =
				    linphone_core_create_conference_scheduler(manager_editing->lc);
				linphone_conference_scheduler_set_account(conference_scheduler, editing_account);
				LinphoneConferenceSchedulerCbs *cbs =
				    linphone_factory_create_conference_scheduler_cbs(linphone_factory_get());
				linphone_conference_scheduler_cbs_set_state_changed(cbs, conference_scheduler_state_changed);
				linphone_conference_scheduler_cbs_set_invitations_sent(cbs, conference_scheduler_invitations_sent);
				linphone_conference_scheduler_add_callbacks(conference_scheduler, cbs);
				linphone_conference_scheduler_cbs_unref(cbs);
				linphone_conference_scheduler_set_info(conference_scheduler, conf_info);

				if (use_default_account) {
					BC_ASSERT_TRUE(wait_for_list(coresList,
					                             &manager_editing->stat.number_of_ConferenceSchedulerStateUpdating,
					                             manager_editing_stat.number_of_ConferenceSchedulerStateUpdating + 1,
					                             liblinphone_tester_sip_timeout));
					BC_ASSERT_TRUE(wait_for_list(coresList,
					                             &manager_editing->stat.number_of_ConferenceSchedulerStateReady,
					                             manager_editing_stat.number_of_ConferenceSchedulerStateReady + 1,
					                             liblinphone_tester_sip_timeout));

					BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneCallIncomingReceived,
					                             focus_stat.number_of_LinphoneCallIncomingReceived + 1,
					                             liblinphone_tester_sip_timeout));
					BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneCallStreamsRunning,
					                             focus_stat.number_of_LinphoneCallStreamsRunning + 1,
					                             liblinphone_tester_sip_timeout));
					BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneCallEnd,
					                             focus_stat.number_of_LinphoneCallEnd + 1,
					                             liblinphone_tester_sip_timeout));
					BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneCallReleased,
					                             focus_stat.number_of_LinphoneCallReleased + 1,
					                             liblinphone_tester_sip_timeout));

					LinphoneChatRoomParams *chat_room_params =
					    linphone_core_create_default_chat_room_params(manager_editing->lc);
					linphone_chat_room_params_set_backend(chat_room_params, LinphoneChatRoomBackendBasic);
					linphone_conference_scheduler_send_invitations(conference_scheduler, chat_room_params);
					linphone_chat_room_params_unref(chat_room_params);

					BC_ASSERT_TRUE(wait_for_list(coresList,
					                             &manager_editing->stat.number_of_ConferenceSchedulerInvitationsSent,
					                             manager_editing_stat.number_of_ConferenceSchedulerInvitationsSent + 1,
					                             liblinphone_tester_sip_timeout));

					for (auto mgr :
					     {focus.getCMgr(), marie.getCMgr(), laure.getCMgr(), pauline.getCMgr(), michelle.getCMgr()}) {
						auto old_stats = participant_stats.front();
						if ((mgr != focus.getCMgr()) && (mgr != manager_editing)) {
							BC_ASSERT_TRUE(wait_for_list(coresList, &mgr->stat.number_of_LinphoneMessageReceived,
							                             old_stats.number_of_LinphoneMessageReceived + 1,
							                             liblinphone_tester_sip_timeout));
							if (!linphone_core_conference_ics_in_message_body_enabled(manager_editing->lc)) {
								BC_ASSERT_TRUE(wait_for_list(coresList,
								                             &mgr->stat.number_of_LinphoneMessageReceivedWithFile,
								                             old_stats.number_of_LinphoneMessageReceivedWithFile + 1,
								                             liblinphone_tester_sip_timeout));
							}

							BC_ASSERT_PTR_NOT_NULL(mgr->stat.last_received_chat_message);
							if (mgr->stat.last_received_chat_message != NULL) {
								const string expected = ContentType::Icalendar.getMediaType();
								BC_ASSERT_STRING_EQUAL(
								    linphone_chat_message_get_content_type(mgr->stat.last_received_chat_message),
								    expected.c_str());
							}

							bctbx_list_t *participant_chat_room_participants =
							    bctbx_list_append(NULL, manager_editing->identity);
							LinphoneChatRoom *pcr = linphone_core_search_chat_room(mgr->lc, NULL, mgr->identity, NULL,
							                                                       participant_chat_room_participants);
							bctbx_list_free(participant_chat_room_participants);
							BC_ASSERT_PTR_NOT_NULL(pcr);
							bctbx_list_t *chat_room_participants = bctbx_list_append(NULL, mgr->identity);

							LinphoneChatRoom *cr = linphone_core_search_chat_room(
							    manager_editing->lc, NULL, manager_editing->identity, NULL, chat_room_participants);
							bctbx_list_free(chat_room_participants);
							BC_ASSERT_PTR_NOT_NULL(cr);

							BC_ASSERT_EQUAL((int)bctbx_list_size(linphone_core_get_chat_rooms(mgr->lc)),
							                (from_organizer || (mgr == michelle.getCMgr())) ? 1 : 2, int, "%d");

							if (cr) {
								LinphoneChatMessage *msg = linphone_chat_room_get_last_message_in_history(cr);
								linphone_chat_room_unref(cr);

								const bctbx_list_t *original_contents = linphone_chat_message_get_contents(msg);
								BC_ASSERT_EQUAL((int)bctbx_list_size(original_contents), 1, int, "%d");
								LinphoneContent *original_content =
								    (LinphoneContent *)bctbx_list_get_data(original_contents);
								if (BC_ASSERT_PTR_NOT_NULL(original_content)) {
									LinphoneConferenceInfo *conf_info_from_original_content =
									    linphone_factory_create_conference_info_from_icalendar_content(
									        linphone_factory_get(), original_content);
									if (BC_ASSERT_PTR_NOT_NULL(conf_info_from_original_content)) {
										BC_ASSERT_TRUE(linphone_address_weak_equal(
										    marie.getCMgr()->identity,
										    linphone_conference_info_get_organizer(conf_info_from_original_content)));
										BC_ASSERT_TRUE(linphone_address_weak_equal(
										    confAddr,
										    linphone_conference_info_get_uri(conf_info_from_original_content)));

										bctbx_list_t *ics_participants =
										    linphone_conference_info_get_participants(conf_info_from_original_content);
										BC_ASSERT_EQUAL(bctbx_list_size(ics_participants), ics_participant_number,
										                size_t, "%zu");
										bctbx_list_free(ics_participants);

										if (start_time > 0) {
											BC_ASSERT_EQUAL((long long)linphone_conference_info_get_date_time(
											                    conf_info_from_original_content),
											                (long long)start_time, long long, "%lld");
											if (end_time > 0) {
												const int duration_m = linphone_conference_info_get_duration(
												    conf_info_from_original_content);
												const int duration_s = duration_m * 60;
												BC_ASSERT_EQUAL(duration_s, (int)(end_time - start_time), int, "%d");
												BC_ASSERT_EQUAL(duration_m, duration, int, "%d");
											}
										}
										if (subject) {
											BC_ASSERT_STRING_EQUAL(
											    linphone_conference_info_get_subject(conf_info_from_original_content),
											    subject);
										} else {
											BC_ASSERT_PTR_NULL(
											    linphone_conference_info_get_subject(conf_info_from_original_content));
										}
										if (description2) {
											BC_ASSERT_STRING_EQUAL(linphone_conference_info_get_description(
											                           conf_info_from_original_content),
											                       description2);
										} else {
											BC_ASSERT_PTR_NULL(linphone_conference_info_get_description(
											    conf_info_from_original_content));
										}
										BC_ASSERT_STRING_EQUAL(
										    linphone_conference_info_get_ics_uid(conf_info_from_original_content), uid);
										const unsigned int ics_sequence =
										    linphone_conference_info_get_ics_sequence(conf_info_from_original_content);
										int exp_sequence = 0;
										if (mgr == michelle.getCMgr()) {
											if (add) {
												exp_sequence = 0;
											} else {
												exp_sequence = 1;
											}
										} else {
											exp_sequence = (sequence + attempt + 1);
										}

										BC_ASSERT_EQUAL(ics_sequence, exp_sequence, int, "%d");

										LinphoneConferenceInfoState exp_state = LinphoneConferenceInfoStateNew;

										if (mgr == michelle.getCMgr()) {
											if (add) {
												exp_state = LinphoneConferenceInfoStateNew;
											} else {
												exp_state = LinphoneConferenceInfoStateCancelled;
											}
										} else {
											exp_state = LinphoneConferenceInfoStateUpdated;
										}
										BC_ASSERT_EQUAL(
										    (int)linphone_conference_info_get_state(conf_info_from_original_content),
										    (int)exp_state, int, "%d");

										linphone_conference_info_unref(conf_info_from_original_content);
									}
								}
								linphone_chat_message_unref(msg);
							}
						}
						participant_stats.pop_front();
					}
				} else {
					BC_ASSERT_TRUE(wait_for_list(coresList,
					                             &manager_editing->stat.number_of_ConferenceSchedulerStateError,
					                             manager_editing_stat.number_of_ConferenceSchedulerStateError + 1,
					                             liblinphone_tester_sip_timeout));
				}
				linphone_conference_scheduler_unref(conference_scheduler);

				for (auto mgr :
				     {focus.getCMgr(), marie.getCMgr(), laure.getCMgr(), pauline.getCMgr(), michelle.getCMgr()}) {
					LinphoneConferenceInfo *info =
					    linphone_core_find_conference_information_from_uri(mgr->lc, confAddr);
					if (!use_default_account && (mgr == michelle.getCMgr())) {
						BC_ASSERT_PTR_NULL(info);
					} else if (BC_ASSERT_PTR_NOT_NULL(info)) {

						const char *exp_subject = NULL;
						if (use_default_account) {
							exp_subject = subject;
						} else {
							exp_subject = initialSubject;
						}

						const char *exp_description = NULL;
						if (mgr != focus.getCMgr()) {
							if (use_default_account) {
								exp_description = description2;
							} else {
								exp_description = description;
							}
						}

						unsigned int exp_sequence = 0;
						LinphoneConferenceInfoState exp_state = LinphoneConferenceInfoStateNew;
						if ((mgr == focus.getCMgr()) || !use_default_account) {
							exp_sequence = 0;
							exp_state = use_default_account ? LinphoneConferenceInfoStateUpdated
							                                : LinphoneConferenceInfoStateNew;
						} else {
							if (mgr == michelle.getCMgr()) {
								if (add) {
									exp_state = LinphoneConferenceInfoStateNew;
								} else {
									exp_state = LinphoneConferenceInfoStateCancelled;
								}
							} else {
								exp_state = LinphoneConferenceInfoStateUpdated;
							}
							if (mgr == michelle.getCMgr()) {
								if (add) {
									exp_sequence = 0;
								} else {
									exp_sequence = 1;
								}
							} else {
								exp_sequence = (sequence + attempt + 1);
							}
						}
						check_conference_info(mgr, confAddr, marie.getCMgr(),
						                      ((use_default_account && add) ? 3 : 2) + ((join) ? 1 : 0), start_time,
						                      duration, exp_subject, exp_description, exp_sequence, exp_state,
						                      security_level);

						if (mgr != focus.getCMgr()) {
							for (auto &p : participants) {
								int exp_participant_sequence = 0;
								// If not using the default account (which was used to create the conference), the
								// conference scheduler errors out and Michelle is not added
								if ((use_default_account) || (p != michelle.getCMgr())) {
									if ((!use_default_account) || (p == michelle.getCMgr())) {
										exp_participant_sequence = 0;
									} else {
										exp_participant_sequence = attempt + 1;
									}
									linphone_conference_info_check_participant(info, p->identity,
									                                           exp_participant_sequence);
								}
							}
							int exp_organizer_sequence = 0;
							if (use_default_account) {
								exp_organizer_sequence = attempt + 1;
							} else {
								exp_organizer_sequence = 0;
							}
							linphone_conference_info_check_organizer(info, exp_organizer_sequence);
						}
					}
					if (info) {
						linphone_conference_info_unref(info);
					}
				}
				linphone_conference_info_unref(conf_info);
			}
			add = !add;
		}
		ms_free(uid);
		linphone_address_unref(alternative_address);
		linphone_address_unref(confAddr);
		bctbx_list_free(coresList);
	}
}

static void organizer_edits_simple_conference(void) {
	edit_simple_conference_base(TRUE, TRUE, FALSE, FALSE, TRUE, FALSE, LinphoneConferenceSecurityLevelNone);
}
static void organizer_edits_simple_conference_using_different_account(void) {
	edit_simple_conference_base(TRUE, FALSE, TRUE, FALSE, FALSE, FALSE, LinphoneConferenceSecurityLevelNone);
}

static void organizer_edits_simple_conference_while_active(void) {
	edit_simple_conference_base(TRUE, TRUE, FALSE, TRUE, TRUE, FALSE, LinphoneConferenceSecurityLevelNone);
}

static void participant_edits_simple_conference(void) {
	edit_simple_conference_base(FALSE, TRUE, TRUE, FALSE, FALSE, FALSE, LinphoneConferenceSecurityLevelNone);
}

static void participant_edits_simple_conference_using_different_account(void) {
	edit_simple_conference_base(FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, LinphoneConferenceSecurityLevelNone);
}

static void organizer_edits_simple_conference_with_server_restart(void) {
	edit_simple_conference_base(TRUE, TRUE, FALSE, FALSE, TRUE, TRUE, LinphoneConferenceSecurityLevelNone);
}

static void conference_edition_with_simultaneous_participant_add_remove_base(bool_t codec_mismatch) {
	Focus focus("chloe_rc");
	{ // to make sure focus is destroyed after clients.
		linphone_core_enable_lime_x3dh(focus.getLc(), true);

		ClientConference marie("marie_rc", focus.getIdentity(), true);
		ClientConference pauline("pauline_rc", focus.getIdentity(), true);
		ClientConference laure("laure_tcp_rc", focus.getIdentity(), true);
		ClientConference michelle("michelle_rc", focus.getIdentity(), true);

		focus.registerAsParticipantDevice(marie);
		focus.registerAsParticipantDevice(pauline);
		focus.registerAsParticipantDevice(laure);
		focus.registerAsParticipantDevice(michelle);

		setup_conference_info_cbs(marie.getCMgr());
		linphone_core_set_file_transfer_server(marie.getLc(), file_transfer_url);

		bctbx_list_t *coresList = bctbx_list_append(NULL, focus.getLc());
		coresList = bctbx_list_append(coresList, marie.getLc());
		coresList = bctbx_list_append(coresList, pauline.getLc());
		coresList = bctbx_list_append(coresList, laure.getLc());
		coresList = bctbx_list_append(coresList, michelle.getLc());

		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_X3dhUserCreationSuccess, 1,
		                             x3dhServer_creationTimeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_X3dhUserCreationSuccess, 1,
		                             x3dhServer_creationTimeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_X3dhUserCreationSuccess, 1,
		                             x3dhServer_creationTimeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_X3dhUserCreationSuccess, 1,
		                             x3dhServer_creationTimeout));

		BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(marie.getLc()));
		BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(pauline.getLc()));
		BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(laure.getLc()));
		BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(michelle.getLc()));

		for (auto mgr : {focus.getCMgr(), marie.getCMgr(), pauline.getCMgr(), laure.getCMgr(), michelle.getCMgr()}) {
			if (codec_mismatch) {
				if (mgr == marie.getCMgr()) {
					disable_all_audio_codecs_except_one(mgr->lc, "pcmu", -1);
				} else {
					disable_all_audio_codecs_except_one(mgr->lc, "pcma", -1);
				}
			}
		}

		std::list<LinphoneCoreManager *> participants{pauline.getCMgr(), marie.getCMgr(), laure.getCMgr()};

		time_t start_time = time(NULL) + 600; // Start in 10 minutes
		int duration = 60;                    // minutes
		time_t end_time = (duration <= 0) ? -1 : (start_time + duration * 60);
		const char *initialSubject = "Test characters: <S-F12><S-F11><S-F6> £$%§";
		const char *description = "Testing characters";
		LinphoneConferenceSecurityLevel security_level = LinphoneConferenceSecurityLevelNone;

		LinphoneAddress *confAddr = create_conference_on_server(focus, marie, participants, start_time, end_time,
		                                                        initialSubject, description, TRUE, security_level);
		BC_ASSERT_PTR_NOT_NULL(confAddr);

		// Chat room creation to send ICS
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneConferenceStateCreated, 2,
		                             liblinphone_tester_sip_timeout));

		const char *subject = "Test characters: <S-F12><S-F11><S-F6> £$%§ (+edits)";
		const char *description2 = "Testing characters (+edits)";

		stats marie_stat = marie.getStats();
		LinphoneAccount *editing_account = linphone_core_get_default_account(marie.getLc());
		BC_ASSERT_PTR_NOT_NULL(editing_account);

		char *uid = NULL;
		unsigned int sequence = 0;
		LinphoneConferenceInfo *info = linphone_core_find_conference_information_from_uri(marie.getLc(), confAddr);
		if (BC_ASSERT_PTR_NOT_NULL(info)) {
			uid = ms_strdup(linphone_conference_info_get_ics_uid(info));
			BC_ASSERT_PTR_NOT_NULL(uid);
			sequence = linphone_conference_info_get_ics_sequence(info);
			linphone_conference_info_unref(info);
		}

		char *conference_address_str = (confAddr) ? linphone_address_as_string(confAddr) : ms_strdup("<unknown>");
		ms_message("%s is trying to update conference %s - adding %s and removing %s",
		           linphone_core_get_identity(marie.getLc()), conference_address_str,
		           linphone_core_get_identity(michelle.getLc()), linphone_core_get_identity(laure.getLc()));
		ms_free(conference_address_str);

		stats focus_stat = focus.getStats();

		LinphoneConferenceInfo *conf_info = linphone_core_find_conference_information_from_uri(marie.getLc(), confAddr);
		linphone_conference_info_set_subject(conf_info, subject);
		linphone_conference_info_set_description(conf_info, description2);
		linphone_conference_info_add_participant(conf_info, michelle.getCMgr()->identity);
		linphone_conference_info_remove_participant(conf_info, laure.getCMgr()->identity);

		bctbx_list_t *ics_participants = linphone_conference_info_get_participants(conf_info);
		BC_ASSERT_EQUAL(bctbx_list_size(ics_participants), participants.size(), size_t, "%zu");
		bctbx_list_free(ics_participants);

		std::list<stats> participant_stats;
		for (auto mgr : {focus.getCMgr(), marie.getCMgr(), laure.getCMgr(), pauline.getCMgr(), michelle.getCMgr()}) {
			participant_stats.push_back(mgr->stat);
		}

		LinphoneConferenceScheduler *conference_scheduler = linphone_core_create_conference_scheduler(marie.getLc());
		linphone_conference_scheduler_set_account(conference_scheduler, editing_account);
		LinphoneConferenceSchedulerCbs *cbs = linphone_factory_create_conference_scheduler_cbs(linphone_factory_get());
		linphone_conference_scheduler_cbs_set_state_changed(cbs, conference_scheduler_state_changed);
		linphone_conference_scheduler_cbs_set_invitations_sent(cbs, conference_scheduler_invitations_sent);
		linphone_conference_scheduler_add_callbacks(conference_scheduler, cbs);
		linphone_conference_scheduler_cbs_unref(cbs);
		linphone_conference_scheduler_set_info(conference_scheduler, conf_info);

		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_ConferenceSchedulerStateUpdating,
		                             marie_stat.number_of_ConferenceSchedulerStateUpdating + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_ConferenceSchedulerStateReady,
		                             marie_stat.number_of_ConferenceSchedulerStateReady + 1,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneCallIncomingReceived,
		                             focus_stat.number_of_LinphoneCallIncomingReceived + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneCallStreamsRunning,
		                             focus_stat.number_of_LinphoneCallStreamsRunning + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneCallEnd,
		                             focus_stat.number_of_LinphoneCallEnd + 1, liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneCallReleased,
		                             focus_stat.number_of_LinphoneCallReleased + 1, liblinphone_tester_sip_timeout));

		LinphoneChatRoomParams *chat_room_params = linphone_core_create_default_chat_room_params(marie.getLc());
		linphone_chat_room_params_set_subject(chat_room_params, "Conference");
		linphone_chat_room_params_enable_encryption(chat_room_params, TRUE);
		linphone_chat_room_params_set_backend(chat_room_params, LinphoneChatRoomBackendFlexisipChat);
		linphone_conference_scheduler_send_invitations(conference_scheduler, chat_room_params);

		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_ConferenceSchedulerInvitationsSent,
		                             marie_stat.number_of_ConferenceSchedulerInvitationsSent + 1,
		                             liblinphone_tester_sip_timeout));

		for (auto mgr : {focus.getCMgr(), marie.getCMgr(), laure.getCMgr(), pauline.getCMgr(), michelle.getCMgr()}) {
			auto old_stats = participant_stats.front();
			if ((mgr != focus.getCMgr()) && (mgr != marie.getCMgr())) {
				BC_ASSERT_TRUE(wait_for_list(coresList, &mgr->stat.number_of_LinphoneMessageReceived,
				                             old_stats.number_of_LinphoneMessageReceived + 1,
				                             liblinphone_tester_sip_timeout));
				if (!linphone_core_conference_ics_in_message_body_enabled(marie.getLc())) {
					BC_ASSERT_TRUE(wait_for_list(coresList, &mgr->stat.number_of_LinphoneMessageReceivedWithFile,
					                             old_stats.number_of_LinphoneMessageReceivedWithFile + 1,
					                             liblinphone_tester_sip_timeout));
				}

				BC_ASSERT_PTR_NOT_NULL(mgr->stat.last_received_chat_message);
				if (mgr->stat.last_received_chat_message != NULL) {
					const string expected = ContentType::Icalendar.getMediaType();
					BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_content_type(mgr->stat.last_received_chat_message),
					                       expected.c_str());
				}

				bctbx_list_t *participant_chat_room_participants = bctbx_list_append(NULL, marie.getCMgr()->identity);
				LinphoneChatRoom *pcr = linphone_core_search_chat_room(mgr->lc, chat_room_params, mgr->identity, NULL,
				                                                       participant_chat_room_participants);
				bctbx_list_free(participant_chat_room_participants);
				BC_ASSERT_PTR_NOT_NULL(pcr);
				bctbx_list_t *chat_room_participants = bctbx_list_append(NULL, mgr->identity);

				LinphoneChatRoom *cr = linphone_core_search_chat_room(
				    marie.getLc(), chat_room_params, marie.getCMgr()->identity, NULL, chat_room_participants);
				bctbx_list_free(chat_room_participants);
				BC_ASSERT_PTR_NOT_NULL(cr);

				BC_ASSERT_EQUAL((int)bctbx_list_size(linphone_core_get_chat_rooms(mgr->lc)),
				                (mgr == michelle.getCMgr()) ? 1 : 2, int, "%d");

				if (cr) {
					LinphoneChatMessage *msg = linphone_chat_room_get_last_message_in_history(cr);
					linphone_chat_room_unref(cr);

					const bctbx_list_t *original_contents = linphone_chat_message_get_contents(msg);
					BC_ASSERT_EQUAL((int)bctbx_list_size(original_contents), 1, int, "%d");
					LinphoneContent *original_content = (LinphoneContent *)bctbx_list_get_data(original_contents);
					if (BC_ASSERT_PTR_NOT_NULL(original_content)) {
						LinphoneConferenceInfo *conf_info_from_original_content =
						    linphone_factory_create_conference_info_from_icalendar_content(linphone_factory_get(),
						                                                                   original_content);
						if (BC_ASSERT_PTR_NOT_NULL(conf_info_from_original_content)) {
							BC_ASSERT_TRUE(linphone_address_weak_equal(
							    marie.getCMgr()->identity,
							    linphone_conference_info_get_organizer(conf_info_from_original_content)));
							BC_ASSERT_TRUE(linphone_address_weak_equal(
							    confAddr, linphone_conference_info_get_uri(conf_info_from_original_content)));

							bctbx_list_t *ics_participants =
							    linphone_conference_info_get_participants(conf_info_from_original_content);
							BC_ASSERT_EQUAL(bctbx_list_size(ics_participants), participants.size(), size_t, "%zu");
							bctbx_list_free(ics_participants);

							if (start_time > 0) {
								BC_ASSERT_EQUAL(
								    (long long)linphone_conference_info_get_date_time(conf_info_from_original_content),
								    (long long)start_time, long long, "%lld");
								if (end_time > 0) {
									const int duration_m =
									    linphone_conference_info_get_duration(conf_info_from_original_content);
									const int duration_s = duration_m * 60;
									BC_ASSERT_EQUAL(duration_s, (int)(end_time - start_time), int, "%d");
									BC_ASSERT_EQUAL(duration_m, duration, int, "%d");
								}
							}
							if (subject) {
								BC_ASSERT_STRING_EQUAL(
								    linphone_conference_info_get_subject(conf_info_from_original_content), subject);
							} else {
								BC_ASSERT_PTR_NULL(
								    linphone_conference_info_get_subject(conf_info_from_original_content));
							}
							if (description2) {
								BC_ASSERT_STRING_EQUAL(
								    linphone_conference_info_get_description(conf_info_from_original_content),
								    description2);
							} else {
								BC_ASSERT_PTR_NULL(
								    linphone_conference_info_get_description(conf_info_from_original_content));
							}
							BC_ASSERT_STRING_EQUAL(
							    linphone_conference_info_get_ics_uid(conf_info_from_original_content), uid);
							const unsigned int ics_sequence =
							    linphone_conference_info_get_ics_sequence(conf_info_from_original_content);
							BC_ASSERT_EQUAL(ics_sequence, (mgr == michelle.getCMgr()) ? 0 : (sequence + 1), int, "%d");

							LinphoneConferenceInfoState exp_state = LinphoneConferenceInfoStateNew;
							if (mgr == laure.getCMgr()) {
								exp_state = LinphoneConferenceInfoStateCancelled;
							} else if (mgr == michelle.getCMgr()) {
								exp_state = LinphoneConferenceInfoStateNew;
							} else {
								exp_state = LinphoneConferenceInfoStateUpdated;
							}
							BC_ASSERT_EQUAL((int)linphone_conference_info_get_state(conf_info_from_original_content),
							                (int)exp_state, int, "%d");

							linphone_conference_info_unref(conf_info_from_original_content);
						}
					}
					linphone_chat_message_unref(msg);
				}
			}
			participant_stats.pop_front();
		}
		linphone_chat_room_params_unref(chat_room_params);
		linphone_conference_scheduler_unref(conference_scheduler);

		for (auto mgr : {focus.getCMgr(), marie.getCMgr(), laure.getCMgr(), pauline.getCMgr(), michelle.getCMgr()}) {
			LinphoneConferenceInfo *info = linphone_core_find_conference_information_from_uri(mgr->lc, confAddr);
			if (BC_ASSERT_PTR_NOT_NULL(info)) {

				const char *exp_subject = subject;

				const char *exp_description = NULL;
				if (mgr != focus.getCMgr()) {
					exp_description = description2;
				}

				unsigned int exp_sequence = 0;
				LinphoneConferenceInfoState exp_state = LinphoneConferenceInfoStateNew;
				if (mgr == focus.getCMgr()) {
					exp_sequence = 0;
					exp_state = LinphoneConferenceInfoStateUpdated;
				} else {
					exp_sequence = (mgr == michelle.getCMgr()) ? 0 : (sequence + 1);
					if (mgr == laure.getCMgr()) {
						exp_state = LinphoneConferenceInfoStateCancelled;
					} else if (mgr == michelle.getCMgr()) {
						exp_state = LinphoneConferenceInfoStateNew;
					} else {
						exp_state = LinphoneConferenceInfoStateUpdated;
					}
				}
				check_conference_info(mgr, confAddr, marie.getCMgr(), participants.size(), start_time, duration,
				                      exp_subject, exp_description, exp_sequence, exp_state, security_level);
			}
			if (info) {
				linphone_conference_info_unref(info);
			}
		}
		linphone_conference_info_unref(conf_info);
		ms_free(uid);
		linphone_address_unref(confAddr);
		bctbx_list_free(coresList);
	}
}

static void conference_edition_with_simultaneous_participant_add_remove(void) {
	conference_edition_with_simultaneous_participant_add_remove_base(FALSE);
}

static void conference_edition_with_organizer_codec_mismatch(void) {
	conference_edition_with_simultaneous_participant_add_remove_base(TRUE);
}

static void conference_cancelled_through_edit_base(bool_t server_restart) {
	Focus focus("chloe_rc");
	{ // to make sure focus is destroyed after clients.
		ClientConference marie("marie_rc", focus.getIdentity());
		ClientConference pauline("pauline_rc", focus.getIdentity());
		ClientConference laure("laure_tcp_rc", focus.getIdentity());
		ClientConference michelle("michelle_rc", focus.getIdentity());

		focus.registerAsParticipantDevice(marie);
		focus.registerAsParticipantDevice(pauline);
		focus.registerAsParticipantDevice(laure);
		focus.registerAsParticipantDevice(michelle);

		setup_conference_info_cbs(marie.getCMgr());
		linphone_core_set_file_transfer_server(marie.getLc(), file_transfer_url);

		bctbx_list_t *coresList = bctbx_list_append(NULL, focus.getLc());
		coresList = bctbx_list_append(coresList, marie.getLc());
		coresList = bctbx_list_append(coresList, pauline.getLc());
		coresList = bctbx_list_append(coresList, laure.getLc());
		coresList = bctbx_list_append(coresList, michelle.getLc());

		std::list<LinphoneCoreManager *> participants{michelle.getCMgr(), pauline.getCMgr(), laure.getCMgr()};

		time_t start_time = time(NULL) + 600; // Start in 10 minutes
		int duration = 60;                    // minutes
		time_t end_time = (duration <= 0) ? -1 : (start_time + duration * 60);
		const char *initialSubject = "Test characters: <S-F12><S-F11><S-F6> £$%§";
		const char *description = "Testing characters";
		LinphoneConferenceSecurityLevel security_level = LinphoneConferenceSecurityLevelNone;

		LinphoneAddress *confAddr = create_conference_on_server(focus, marie, participants, start_time, end_time,
		                                                        initialSubject, description, TRUE, security_level);
		BC_ASSERT_PTR_NOT_NULL(confAddr);

		// Chat room creation to send ICS
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneConferenceStateCreated, 2,
		                             liblinphone_tester_sip_timeout));

		char *uid = NULL;
		unsigned int sequence = 0;
		LinphoneConferenceInfo *info = linphone_core_find_conference_information_from_uri(marie.getLc(), confAddr);
		if (BC_ASSERT_PTR_NOT_NULL(info)) {
			uid = ms_strdup(linphone_conference_info_get_ics_uid(info));
			BC_ASSERT_PTR_NOT_NULL(uid);
			sequence = linphone_conference_info_get_ics_sequence(info);
			linphone_conference_info_unref(info);
		}

		stats focus_stat = focus.getStats();
		stats manager_editing_stat = marie.getStats();

		std::list<stats> participant_stats;
		for (auto mgr : {focus.getCMgr(), marie.getCMgr(), laure.getCMgr(), pauline.getCMgr(), michelle.getCMgr()}) {
			participant_stats.push_back(mgr->stat);
		}

		char *conference_address_str = (confAddr) ? linphone_address_as_string(confAddr) : ms_strdup("<unknown>");
		ms_message("%s is trying to change duration of conference %s", linphone_core_get_identity(marie.getLc()),
		           conference_address_str);
		LinphoneConferenceInfo *conf_info = linphone_core_find_conference_information_from_uri(marie.getLc(), confAddr);
		unsigned int new_duration = 2000;
		linphone_conference_info_set_duration(conf_info, new_duration);

		LinphoneConferenceScheduler *conference_scheduler = linphone_core_create_conference_scheduler(marie.getLc());
		LinphoneAccount *editing_account = linphone_core_get_default_account(marie.getLc());
		BC_ASSERT_PTR_NOT_NULL(editing_account);
		linphone_conference_scheduler_set_account(conference_scheduler, editing_account);
		LinphoneConferenceSchedulerCbs *cbs = linphone_factory_create_conference_scheduler_cbs(linphone_factory_get());
		linphone_conference_scheduler_cbs_set_state_changed(cbs, conference_scheduler_state_changed);
		linphone_conference_scheduler_cbs_set_invitations_sent(cbs, conference_scheduler_invitations_sent);
		linphone_conference_scheduler_add_callbacks(conference_scheduler, cbs);
		linphone_conference_scheduler_cbs_unref(cbs);
		cbs = nullptr;
		linphone_conference_scheduler_set_info(conference_scheduler, conf_info);

		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_ConferenceSchedulerStateUpdating,
		                             manager_editing_stat.number_of_ConferenceSchedulerStateUpdating + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_ConferenceSchedulerStateReady,
		                             manager_editing_stat.number_of_ConferenceSchedulerStateReady + 1,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneCallIncomingReceived,
		                             focus_stat.number_of_LinphoneCallIncomingReceived + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneCallStreamsRunning,
		                             focus_stat.number_of_LinphoneCallStreamsRunning + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneCallEnd,
		                             focus_stat.number_of_LinphoneCallEnd + 1, liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneCallReleased,
		                             focus_stat.number_of_LinphoneCallReleased + 1, liblinphone_tester_sip_timeout));

		LinphoneChatRoomParams *chat_room_params = linphone_core_create_default_chat_room_params(marie.getLc());
		linphone_chat_room_params_set_backend(chat_room_params, LinphoneChatRoomBackendBasic);
		linphone_conference_scheduler_send_invitations(conference_scheduler, chat_room_params);
		linphone_chat_room_params_unref(chat_room_params);
		chat_room_params = nullptr;

		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_ConferenceSchedulerInvitationsSent,
		                             manager_editing_stat.number_of_ConferenceSchedulerInvitationsSent + 1,
		                             liblinphone_tester_sip_timeout));

		for (auto mgr : {focus.getCMgr(), marie.getCMgr(), laure.getCMgr(), pauline.getCMgr(), michelle.getCMgr()}) {
			auto old_stats = participant_stats.front();
			if ((mgr != focus.getCMgr()) && (mgr != marie.getCMgr())) {
				BC_ASSERT_TRUE(wait_for_list(coresList, &mgr->stat.number_of_LinphoneMessageReceived,
				                             old_stats.number_of_LinphoneMessageReceived + 1,
				                             liblinphone_tester_sip_timeout));
				if (!linphone_core_conference_ics_in_message_body_enabled(marie.getLc())) {
					BC_ASSERT_TRUE(wait_for_list(coresList, &mgr->stat.number_of_LinphoneMessageReceivedWithFile,
					                             old_stats.number_of_LinphoneMessageReceivedWithFile + 1,
					                             liblinphone_tester_sip_timeout));
				}

				BC_ASSERT_PTR_NOT_NULL(mgr->stat.last_received_chat_message);
				if (mgr->stat.last_received_chat_message != NULL) {
					const string expected = ContentType::Icalendar.getMediaType();
					BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_content_type(mgr->stat.last_received_chat_message),
					                       expected.c_str());
				}

				bctbx_list_t *participant_chat_room_participants = bctbx_list_append(NULL, marie.getCMgr()->identity);
				LinphoneChatRoom *pcr = linphone_core_search_chat_room(mgr->lc, NULL, mgr->identity, NULL,
				                                                       participant_chat_room_participants);
				bctbx_list_free(participant_chat_room_participants);
				BC_ASSERT_PTR_NOT_NULL(pcr);
				bctbx_list_t *chat_room_participants = bctbx_list_append(NULL, mgr->identity);

				LinphoneChatRoom *cr = linphone_core_search_chat_room(marie.getLc(), NULL, marie.getCMgr()->identity,
				                                                      NULL, chat_room_participants);
				bctbx_list_free(chat_room_participants);
				BC_ASSERT_PTR_NOT_NULL(cr);

				BC_ASSERT_EQUAL((int)bctbx_list_size(linphone_core_get_chat_rooms(mgr->lc)), 1, int, "%d");

				if (cr) {
					LinphoneChatMessage *msg = linphone_chat_room_get_last_message_in_history(cr);
					linphone_chat_room_unref(cr);

					const bctbx_list_t *original_contents = linphone_chat_message_get_contents(msg);
					BC_ASSERT_EQUAL((int)bctbx_list_size(original_contents), 1, int, "%d");
					LinphoneContent *original_content = (LinphoneContent *)bctbx_list_get_data(original_contents);
					if (BC_ASSERT_PTR_NOT_NULL(original_content)) {
						LinphoneConferenceInfo *conf_info_from_original_content =
						    linphone_factory_create_conference_info_from_icalendar_content(linphone_factory_get(),
						                                                                   original_content);
						if (BC_ASSERT_PTR_NOT_NULL(conf_info_from_original_content)) {
							BC_ASSERT_TRUE(linphone_address_weak_equal(
							    marie.getCMgr()->identity,
							    linphone_conference_info_get_organizer(conf_info_from_original_content)));
							BC_ASSERT_TRUE(linphone_address_weak_equal(
							    confAddr, linphone_conference_info_get_uri(conf_info_from_original_content)));

							bctbx_list_t *ics_participants =
							    linphone_conference_info_get_participants(conf_info_from_original_content);
							BC_ASSERT_EQUAL(bctbx_list_size(ics_participants), 3, size_t, "%zu");
							bctbx_list_free(ics_participants);

							if (start_time > 0) {
								BC_ASSERT_EQUAL(
								    (long long)linphone_conference_info_get_date_time(conf_info_from_original_content),
								    (long long)start_time, long long, "%lld");
								if (end_time > 0) {
									const int duration_m =
									    linphone_conference_info_get_duration(conf_info_from_original_content);
									BC_ASSERT_EQUAL(duration_m, new_duration, int, "%d");
								}
							}
							if (initialSubject) {
								BC_ASSERT_STRING_EQUAL(
								    linphone_conference_info_get_subject(conf_info_from_original_content),
								    initialSubject);
							} else {
								BC_ASSERT_PTR_NULL(
								    linphone_conference_info_get_subject(conf_info_from_original_content));
							}
							if (description) {
								BC_ASSERT_STRING_EQUAL(
								    linphone_conference_info_get_description(conf_info_from_original_content),
								    description);
							} else {
								BC_ASSERT_PTR_NULL(
								    linphone_conference_info_get_description(conf_info_from_original_content));
							}
							BC_ASSERT_STRING_EQUAL(
							    linphone_conference_info_get_ics_uid(conf_info_from_original_content), uid);
							const unsigned int ics_sequence =
							    linphone_conference_info_get_ics_sequence(conf_info_from_original_content);
							BC_ASSERT_EQUAL(ics_sequence, (sequence + 1), int, "%d");

							LinphoneConferenceInfoState exp_state = LinphoneConferenceInfoStateUpdated;
							BC_ASSERT_EQUAL((int)linphone_conference_info_get_state(conf_info_from_original_content),
							                (int)exp_state, int, "%d");

							linphone_conference_info_unref(conf_info_from_original_content);
						}
					}
					linphone_chat_message_unref(msg);
				}
			}
			participant_stats.pop_front();
		}
		linphone_conference_scheduler_unref(conference_scheduler);
		conference_scheduler = nullptr;
		linphone_conference_info_unref(conf_info);
		conf_info = nullptr;

		if (server_restart) {
			ms_message("%s is restarting", linphone_core_get_identity(focus.getLc()));
			coresList = bctbx_list_remove(coresList, focus.getLc());
			// Restart flexisip
			focus.reStart();

			LinphoneVideoActivationPolicy *pol =
			    linphone_factory_create_video_activation_policy(linphone_factory_get());
			linphone_video_activation_policy_set_automatically_accept(pol, TRUE);
			linphone_video_activation_policy_set_automatically_initiate(pol, TRUE);
			linphone_core_set_video_activation_policy(focus.getLc(), pol);
			linphone_video_activation_policy_unref(pol);

			linphone_core_enable_video_capture(focus.getLc(), TRUE);
			linphone_core_enable_video_display(focus.getLc(), TRUE);
			coresList = bctbx_list_append(coresList, focus.getLc());
		}

		const char *subject = "Test characters: <S-F12><S-F11><S-F6> £$%§ (+cancelled)";
		const char *description2 = "Testing characters (+cancelled)";

		info = linphone_core_find_conference_information_from_uri(marie.getLc(), confAddr);
		if (BC_ASSERT_PTR_NOT_NULL(info)) {
			sequence = linphone_conference_info_get_ics_sequence(info);
			linphone_conference_info_unref(info);
		}

		ms_message("%s is trying to cancel conference %s", linphone_core_get_identity(marie.getLc()),
		           conference_address_str);
		ms_free(conference_address_str);

		focus_stat = focus.getStats();
		manager_editing_stat = marie.getStats();

		participant_stats.clear();
		for (auto mgr : {focus.getCMgr(), marie.getCMgr(), laure.getCMgr(), pauline.getCMgr(), michelle.getCMgr()}) {
			participant_stats.push_back(mgr->stat);
		}

		conf_info = linphone_core_find_conference_information_from_uri(marie.getLc(), confAddr);
		linphone_conference_info_set_subject(conf_info, subject);
		linphone_conference_info_set_description(conf_info, description2);

		bctbx_list_t *ics_participants = linphone_conference_info_get_participants(conf_info);
		BC_ASSERT_EQUAL(bctbx_list_size(ics_participants), 3, size_t, "%zu");
		bctbx_list_free(ics_participants);

		conference_scheduler = linphone_core_create_conference_scheduler(marie.getLc());
		linphone_conference_scheduler_set_account(conference_scheduler, editing_account);
		cbs = linphone_factory_create_conference_scheduler_cbs(linphone_factory_get());
		linphone_conference_scheduler_cbs_set_state_changed(cbs, conference_scheduler_state_changed);
		linphone_conference_scheduler_cbs_set_invitations_sent(cbs, conference_scheduler_invitations_sent);
		linphone_conference_scheduler_add_callbacks(conference_scheduler, cbs);
		linphone_conference_scheduler_cbs_unref(cbs);
		linphone_conference_scheduler_cancel_conference(conference_scheduler, conf_info);

		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_ConferenceSchedulerStateUpdating,
		                             manager_editing_stat.number_of_ConferenceSchedulerStateUpdating + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_ConferenceSchedulerStateReady,
		                             manager_editing_stat.number_of_ConferenceSchedulerStateReady + 1,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneCallIncomingReceived,
		                             focus_stat.number_of_LinphoneCallIncomingReceived + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneCallStreamsRunning,
		                             focus_stat.number_of_LinphoneCallStreamsRunning + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneCallEnd,
		                             focus_stat.number_of_LinphoneCallEnd + 1, liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneCallReleased,
		                             focus_stat.number_of_LinphoneCallReleased + 1, liblinphone_tester_sip_timeout));

		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneConferenceStateTerminationPending,
		                             focus_stat.number_of_LinphoneConferenceStateTerminationPending + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneConferenceStateTerminated,
		                             focus_stat.number_of_LinphoneConferenceStateTerminated + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneConferenceStateDeleted,
		                             focus_stat.number_of_LinphoneConferenceStateDeleted + 1,
		                             liblinphone_tester_sip_timeout));

		chat_room_params = linphone_core_create_default_chat_room_params(marie.getLc());
		linphone_chat_room_params_set_backend(chat_room_params, LinphoneChatRoomBackendBasic);
		linphone_conference_scheduler_send_invitations(conference_scheduler, chat_room_params);
		linphone_chat_room_params_unref(chat_room_params);

		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_ConferenceSchedulerInvitationsSent,
		                             manager_editing_stat.number_of_ConferenceSchedulerInvitationsSent + 1,
		                             liblinphone_tester_sip_timeout));

		for (auto mgr : {focus.getCMgr(), marie.getCMgr(), laure.getCMgr(), pauline.getCMgr(), michelle.getCMgr()}) {
			auto old_stats = participant_stats.front();
			if ((mgr != focus.getCMgr()) && (mgr != marie.getCMgr())) {
				BC_ASSERT_TRUE(wait_for_list(coresList, &mgr->stat.number_of_LinphoneMessageReceived,
				                             old_stats.number_of_LinphoneMessageReceived + 1,
				                             liblinphone_tester_sip_timeout));
				if (!linphone_core_conference_ics_in_message_body_enabled(marie.getLc())) {
					BC_ASSERT_TRUE(wait_for_list(coresList, &mgr->stat.number_of_LinphoneMessageReceivedWithFile,
					                             old_stats.number_of_LinphoneMessageReceivedWithFile + 1,
					                             liblinphone_tester_sip_timeout));
				}

				BC_ASSERT_PTR_NOT_NULL(mgr->stat.last_received_chat_message);
				if (mgr->stat.last_received_chat_message != NULL) {
					const string expected = ContentType::Icalendar.getMediaType();
					BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_content_type(mgr->stat.last_received_chat_message),
					                       expected.c_str());
				}

				bctbx_list_t *participant_chat_room_participants = bctbx_list_append(NULL, marie.getCMgr()->identity);
				LinphoneChatRoom *pcr = linphone_core_search_chat_room(mgr->lc, NULL, mgr->identity, NULL,
				                                                       participant_chat_room_participants);
				bctbx_list_free(participant_chat_room_participants);
				BC_ASSERT_PTR_NOT_NULL(pcr);
				bctbx_list_t *chat_room_participants = bctbx_list_append(NULL, mgr->identity);

				LinphoneChatRoom *cr = linphone_core_search_chat_room(marie.getLc(), NULL, marie.getCMgr()->identity,
				                                                      NULL, chat_room_participants);
				bctbx_list_free(chat_room_participants);
				BC_ASSERT_PTR_NOT_NULL(cr);

				BC_ASSERT_EQUAL((int)bctbx_list_size(linphone_core_get_chat_rooms(mgr->lc)), 1, int, "%d");

				if (cr) {
					LinphoneChatMessage *msg = linphone_chat_room_get_last_message_in_history(cr);
					linphone_chat_room_unref(cr);

					const bctbx_list_t *original_contents = linphone_chat_message_get_contents(msg);
					BC_ASSERT_EQUAL((int)bctbx_list_size(original_contents), 1, int, "%d");
					LinphoneContent *original_content = (LinphoneContent *)bctbx_list_get_data(original_contents);
					if (BC_ASSERT_PTR_NOT_NULL(original_content)) {
						LinphoneConferenceInfo *conf_info_from_original_content =
						    linphone_factory_create_conference_info_from_icalendar_content(linphone_factory_get(),
						                                                                   original_content);
						if (BC_ASSERT_PTR_NOT_NULL(conf_info_from_original_content)) {
							BC_ASSERT_TRUE(linphone_address_weak_equal(
							    marie.getCMgr()->identity,
							    linphone_conference_info_get_organizer(conf_info_from_original_content)));
							BC_ASSERT_TRUE(linphone_address_weak_equal(
							    confAddr, linphone_conference_info_get_uri(conf_info_from_original_content)));

							bctbx_list_t *ics_participants =
							    linphone_conference_info_get_participants(conf_info_from_original_content);
							BC_ASSERT_EQUAL(bctbx_list_size(ics_participants), 0, size_t, "%zu");
							bctbx_list_free(ics_participants);

							if (start_time > 0) {
								BC_ASSERT_EQUAL(
								    (long long)linphone_conference_info_get_date_time(conf_info_from_original_content),
								    (long long)start_time, long long, "%lld");
								if (end_time > 0) {
									const int duration_m =
									    linphone_conference_info_get_duration(conf_info_from_original_content);
									BC_ASSERT_EQUAL(duration_m, new_duration, int, "%d");
								}
							}
							if (subject) {
								BC_ASSERT_STRING_EQUAL(
								    linphone_conference_info_get_subject(conf_info_from_original_content), subject);
							} else {
								BC_ASSERT_PTR_NULL(
								    linphone_conference_info_get_subject(conf_info_from_original_content));
							}
							if (description2) {
								BC_ASSERT_STRING_EQUAL(
								    linphone_conference_info_get_description(conf_info_from_original_content),
								    description2);
							} else {
								BC_ASSERT_PTR_NULL(
								    linphone_conference_info_get_description(conf_info_from_original_content));
							}
							BC_ASSERT_STRING_EQUAL(
							    linphone_conference_info_get_ics_uid(conf_info_from_original_content), uid);
							const unsigned int ics_sequence =
							    linphone_conference_info_get_ics_sequence(conf_info_from_original_content);
							BC_ASSERT_EQUAL(ics_sequence, (sequence + 1), int, "%d");

							LinphoneConferenceInfoState exp_state = LinphoneConferenceInfoStateNew;
							if (mgr == focus.getCMgr()) {
								exp_state = LinphoneConferenceInfoStateUpdated;
							} else {
								exp_state = LinphoneConferenceInfoStateCancelled;
							}
							BC_ASSERT_EQUAL((int)linphone_conference_info_get_state(conf_info_from_original_content),
							                (int)exp_state, int, "%d");

							linphone_conference_info_unref(conf_info_from_original_content);
						}
					}
					linphone_chat_message_unref(msg);
				}
			}
			participant_stats.pop_front();
		}
		linphone_conference_scheduler_unref(conference_scheduler);

		for (auto mgr : {focus.getCMgr(), marie.getCMgr(), laure.getCMgr(), pauline.getCMgr(), michelle.getCMgr()}) {
			LinphoneConferenceInfo *info = linphone_core_find_conference_information_from_uri(mgr->lc, confAddr);
			if (BC_ASSERT_PTR_NOT_NULL(info)) {

				const char *exp_subject = subject;

				const char *exp_description = NULL;
				if (mgr != focus.getCMgr()) {
					exp_description = description2;
				}

				unsigned int exp_sequence = 0;
				LinphoneConferenceInfoState exp_state = LinphoneConferenceInfoStateCancelled;
				unsigned int exp_participant_number = 0;
				if (mgr == focus.getCMgr()) {
					exp_sequence = 0;
				} else {
					exp_sequence = (sequence + 1);
				}
				check_conference_info(mgr, confAddr, marie.getCMgr(), exp_participant_number, start_time, new_duration,
				                      exp_subject, exp_description, exp_sequence, exp_state, security_level);
			}
			if (info) {
				linphone_conference_info_unref(info);
			}
		}
		linphone_conference_info_unref(conf_info);
		ms_free(uid);
		linphone_address_unref(confAddr);
		bctbx_list_free(coresList);
	}
}

static void conference_cancelled_through_edit(void) {
	conference_cancelled_through_edit_base(FALSE);
}

static void create_conference_with_server_restart_conference_cancelled(void) {
	conference_cancelled_through_edit_base(TRUE);
}

} // namespace LinphoneTest

static test_t local_conference_conference_edition_tests[] = {
    TEST_NO_TAG("Organizer edits simple conference", LinphoneTest::organizer_edits_simple_conference),
    TEST_NO_TAG("Organizer edits simple conference using different account",
                LinphoneTest::organizer_edits_simple_conference_using_different_account),
    TEST_NO_TAG("Organizer edits simple conference while it is active",
                LinphoneTest::organizer_edits_simple_conference_while_active),
    TEST_NO_TAG("Organizer edits simple conference with server restart",
                LinphoneTest::organizer_edits_simple_conference_with_server_restart),
    TEST_NO_TAG("Participant edits simple conference", LinphoneTest::participant_edits_simple_conference),
    TEST_NO_TAG("Participant edits simple conference using different account",
                LinphoneTest::participant_edits_simple_conference_using_different_account),
    TEST_NO_TAG("Conference cancelled through edit", LinphoneTest::conference_cancelled_through_edit),
    TEST_NO_TAG("Conference edition with simultanoues participant added removed",
                LinphoneTest::conference_edition_with_simultaneous_participant_add_remove),
    TEST_NO_TAG("Conference edition with organizer codec mismatch",
                LinphoneTest::conference_edition_with_organizer_codec_mismatch),
    TEST_NO_TAG("Create conference with server restart (conference cancelled)",
                LinphoneTest::create_conference_with_server_restart_conference_cancelled)};

test_suite_t local_conference_test_suite_conference_edition = {"Local conference tester (Conference edition)",
                                                               NULL,
                                                               NULL,
                                                               liblinphone_tester_before_each,
                                                               liblinphone_tester_after_each,
                                                               sizeof(local_conference_conference_edition_tests) /
                                                                   sizeof(local_conference_conference_edition_tests[0]),
                                                               local_conference_conference_edition_tests};
