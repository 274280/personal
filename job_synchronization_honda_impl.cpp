/****************************************************************************
**
** Copyright (C) 2020 Elektrobit Automotive GmbH. All rights reserved.
**
** Information contained herein is subject to change without notice.
** Elektrobit retains ownership and all other rights in the software and each
** component thereof.
** Any reproduction of the software or components thereof without the prior
** written permission of Elektrobit is prohibited.
**
****************************************************************************/
#include "Service/job_synchronization_honda_impl.h"
#include <MicroService/microservice_proxy.h>
#include <Converters/VectorOperations.h>
#include <persistency_plugin_interfaces/persistency.h>

#include "Protocol/http_client.h"
#include "Protocol/mqtt_client.h"
#include "Service/json_web_token_retrieval.h"
#include "Service/thread_safe_map.h"
#include "connectivity_interfaces/protocol/mqtt_interface.h"
#include <Poco/JSON/Parser.h>
#include <Poco/File.h>
#include <logging_core/logger.h>

#include <UdsClientServiceInterfaces/UdsClientService.h>
#include <UdsFrames/Routine/UdsRoutineControlRequest.h>
#include <UdsFrames/Routine/UdsRoutineControlResponse.h>
#include <unistd.h>

#include <functional>
#include <string>
#include <map>
#include <future>
#include <experimental/filesystem>
#include <unistd.h>

namespace cc
{
    namespace service
    {
        namespace
        {

            using cadian::connectivity::Http_interface;
            using cadian::connectivity::Job_synchronization;
            using cadian::connectivity::Mqtt_interface;
            using cadian::connectivity::Status_report;
            using utils::Utils;

            static constexpr uint16_t OTA_APPROVAL_ROUTINE{0XD401};
            static constexpr uint8_t CONDITION_NOT_CORRECT{0X22};
            static constexpr uint16_t FINISH_OTA_ROUTINE{0XD402};
            static constexpr int8_t TIMEOUT{100};
            static constexpr int8_t INVALID_STATE{-1};
            static constexpr auto NRC_ERROR_CODE = "12000";
            static constexpr auto KEEP_ALIVE_ERROR_CODE = "02000";

            void connect_to_mqtt_broker(Mqtt_interface &mqtt_client)
            {
                mqtt_client.connect();
            }

        } // namespace

        Job_synchronization_honda_impl::Job_synchronization_honda_impl(
            Http_interface &http,
            Mqtt_interface &mqtt,
            Status_report &status_report,
            std::pair<int32_t, int32_t> mqtt_qos,
            std::string vehicle_id,
            Thread_safe_map<std::string, Honda::Honda_campaign> &campaign_map,
            std::string TCU_path,
            util::microservice::Proxy<cadian::UDS::UdsClientService> const &udsClient_proxy,
            uint16_t uds_client_id,
            uint16_t vcu_id,
            std::string download_path)
            : m_http_client(http), m_mqtt_client(mqtt), m_status_report(status_report), m_mqtt_qos(std::move(mqtt_qos)), m_vehicle_id(std::move(vehicle_id)), m_campaign_map(campaign_map), m_power_status("off"), m_on_startup(true), m_TCU_path(TCU_path), m_udsClient_proxy(udsClient_proxy), m_uds_client_id(uds_client_id), m_vcu_id(vcu_id), m_download_path(download_path), m_nrc_value(0), m_is_OTA_approval_request_sent(false), m_is_OTA_approval_response_received(false), m_is_OTA_approved(false), m_is_IG_OFF_required(false), m_is_finish_OTA_request_sent(false), m_is_finish_OTA_response_received(false), m_is_finish_OTA_approved(false), m_supervision_timer(0), m_persisted_state(INVALID_STATE)

        {
        }

        void Job_synchronization_honda_impl::OTA_campaign_approved(cadian::UDS::PDU const &response)
        {
            LOG_INFO << "OTA campaign approved by VCU" << std::endl;
            m_is_OTA_approval_response_received = true;
            m_is_OTA_approved = true;

            cadian::UDS::Message response_message{response};
            cadian::UDS::RoutineControlResponse VCU_response(response_message);

            // LOG_DEBUG << "Response for OTA approval is :"
            //       << util::convert::to_hex_string(VCU_response.GetRoutineStatusRecord()) << std::endl;

            (VCU_response.GetRoutineStatusRecord()[0] == 0) ? (m_is_IG_OFF_required = false)
                                                            : (m_is_IG_OFF_required = true);
        }

        void Job_synchronization_honda_impl::OTA_campaign_not_approved(cadian::UDS::PDU const &response)
        {
            LOG_WARN << "OTA campaign NOT approved by VCU" << std::endl;
            m_is_OTA_approval_response_received = true;
            m_is_OTA_approved = false;
            // collect nrc here and store as a class member variable
            m_nrc_value = response[2];
            // for(const auto& response_byte:response)
            // LOG_DEBUG<< "Received ressponse: " << response_byte <<std::endl;
        }

        void Job_synchronization_honda_impl::finish_OTA_approved(cadian::UDS::PDU const &response)
        {
            LOG_INFO << "Finish OTA campaign approved by VCU" << std::endl;
            m_is_finish_OTA_response_received = true;
            m_is_finish_OTA_approved = true;

            cadian::UDS::Message response_message{response};
            cadian::UDS::RoutineControlResponse VCU_response(response_message);
        }

        void Job_synchronization_honda_impl::finish_OTA_not_approved(cadian::UDS::PDU const &response)
        {

            LOG_WARN << "Finish OTA campaign NOT approved by VCU" << std::endl;
            m_is_finish_OTA_response_received = true;
            m_is_finish_OTA_approved = false;
            for (const auto &response_byte : response)
                LOG_DEBUG << "Received ressponse: " << response_byte << std::endl;
        }

        // To send OTA approval request to VCU
        void Job_synchronization_honda_impl::send_VCU_OTA_approval_request()
        {
            cadian::UDS::AddressInformation address_info{m_uds_client_id, m_vcu_id,
                                                         cadian::UDS::AddressType::Physical};

            std::vector<uint8_t> routineControlOptionRecord;
            cadian::UDS::RoutineControlRequest request_VCU_OTA_approval(cadian::UDS::RcLev::StartRoutine, OTA_APPROVAL_ROUTINE, routineControlOptionRecord);
            try
            {
                LOG_INFO << "Sending VCU OTA approval request" << std::endl;
                static_cast<void>(std::async(std::launch::async, [this, address_info, request_VCU_OTA_approval]
                                             { m_udsClient_proxy.get_service()->GenericRequest(
                                                   address_info, request_VCU_OTA_approval.GetPdu(),
                                                   [this](cadian::UDS::AddressInformation const &,
                                                          cadian::UDS::PDU const &responses)
                                                   {
                                                       OTA_campaign_not_approved(responses);
                                                   },

                                                   [this](cadian::UDS::AddressInformation const &,
                                                          cadian::UDS::PDU const &responses)
                                                   {
                                                       OTA_campaign_approved(responses);
                                                   }); }));
            }
            catch (const cadian::UDS::UdsClientException &)
            {
                LOG_ERROR << "Error in sending request to VCU for OTA approval " << std::endl;
            }
        }

        // To send finish OTA request to VCU
        void Job_synchronization_honda_impl::send_VCU_finish_OTA_request()
        {
            cadian::UDS::AddressInformation address_info{m_uds_client_id, m_vcu_id,
                                                         cadian::UDS::AddressType::Physical};

            std::vector<uint8_t> routineControlOptionRecord;
            cadian::UDS::RoutineControlRequest finish_OTA_request(cadian::UDS::RcLev::StartRoutine, FINISH_OTA_ROUTINE, routineControlOptionRecord);
            try
            {
                LOG_INFO << "Sending finish OTA campaign request to VCU" << std::endl;
                static_cast<void>(std::async(std::launch::async, [this, address_info, finish_OTA_request]
                                             { m_udsClient_proxy.get_service()->GenericRequest(
                                                   address_info, finish_OTA_request.GetPdu(),
                                                   [this](cadian::UDS::AddressInformation const &,
                                                          cadian::UDS::PDU const &responses)
                                                   {
                                                       finish_OTA_not_approved(responses);
                                                   },

                                                   [this](cadian::UDS::AddressInformation const &,
                                                          cadian::UDS::PDU const &responses)
                                                   {
                                                       finish_OTA_approved(responses);
                                                   }); }));

                LOG_INFO << "Finish OTA campaign approval request sent" << std::endl;
            }
            catch (const cadian::UDS::UdsClientException &)
            {
                LOG_ERROR << "Error in sending request to VCU for finishing OTA campaign." << std::endl;
            }
        }

        // Mapping of campaign on persistency
        void Job_synchronization_honda_impl::campaign_mapping()
        {

            std::pair<std::string, cc::service::Honda::Honda_campaign> campaign;

            campaign.second.m_vehicle_id = m_mqtt_client.get_Download_request_handler().frame_type_and_number;
            campaign.second.m_campaign_id = m_mqtt_client.get_Download_request_handler().campaignId;
            campaign.second.m_url_overwritten = false;
            campaign.second.m_expected_apply_time = 60;
            for (const auto &ecu : m_mqtt_client.get_Download_request_handler().ecu_list)
            {
                if (ecu.downloadUrlNew.empty())
                {
                    LOG_DEBUG << "No Update package url received for :" << ecu.ecuTypeId << std::endl;
                    continue;
                }
                campaign.second.m_ecus.push_back({std::to_string(std::stoi(ecu.physicalTargetId, nullptr, 16)),
                                                  ecu.canEther,
                                                  ecu.newSwNo,
                                                  ecu.oldSwNo,
                                                  ecu.downloadUrlNew,
                                                  0,
                                                  ecu.downloadUrlOld,
                                                  0,
                                                  ecu.downloadUrlBoot,
                                                  0,
                                                  ecu.softwareVersionNew,
                                                  ecu.softwareVersionOld});
                m_mqtt_client.insert_to_map({std::to_string(std::stoi(ecu.physicalTargetId, nullptr, 16)) + "new", {ecu.downloadUrlNew, ecu.hashNew}});
                m_mqtt_client.insert_to_map({std::to_string(std::stoi(ecu.physicalTargetId, nullptr, 16)) + "rollback", {ecu.downloadUrlOld, ecu.hashOld}});
                // m_mqtt_client.insert_to_map({std::to_string(std::stoi( ecu.physicalTargetId, nullptr, 16))+"rollback", {ecu.downloadUrlBoot,ecu.hashBoot}});
            }
            campaign.second.m_timing_setting = {"immediate", "0011201202", "immediate", "1011201202"};
            campaign.second.m_campaign_condition = {{campaign.second.m_campaign_id}, {}, campaign.second.m_ecus};

            installResult = std::make_shared<Utils::InstallResult>(campaign.second.m_ecus,
                                                                   m_mqtt_client.get_Download_request_handler().ecu_list,
                                                                   m_mqtt_client.get_persistency_service());
            LOG_INFO << "Install result created: " << installResult->to_string() << std::endl;

            LOG_INFO << "Job sync Honda Campaign before map insert " << campaign.second << std::endl;

            m_campaign_map.insert(std::pair<std::string, Honda::Honda_campaign>(campaign.second.m_campaign_id, campaign.second));
            /*
                m_jobs.push_back({
                    campaign.second.m_vehicle_id,
                    campaign.second.m_campaign_id,
                    "ebri:eb:honda.local:campaign-service:campaign:campaigns/"+ campaign.second.m_campaign_id,
                    "ebri:eb:local:honda:update-execution-plan:actions/"+ campaign.second.m_campaign_id,
                    Job_synchronization::Job_state::pending,
                    "null"
                });
            */
            m_mqtt_client.set_new_assembly_available(false);

            std::pair<std::string, cc::service::Honda::Honda_campaign> campaign1;
            if (!m_campaign_map.find(m_mqtt_client.get_Download_request_handler().campaignId, campaign1))
            {
                LOG_INFO << "Job sync find is null " << std::endl;
            }
            LOG_INFO << "Job sync Honda Campaign after map insert " << campaign1.second << std::endl;
            // return Job_synchronization::Response {Job_synchronization::Response::Code::SUCCESS, m_jobs};
        }

        // To handle reloaded persisted state
        std::pair<int8_t, bool> Job_synchronization_honda_impl::persisted_state_handling(int8_t persisted_state)
        {
            bool return_flag = false;
            auto persistency = m_mqtt_client.get_persistency_service();

            if (persisted_state == Utils::PREPARING)
            {
                LOG_INFO << "Reloaded state is Preparing, hence sending OTA approval request to VCU"
                         << std::endl;
                m_mqtt_client.reload_campaign_id();
                send_VCU_OTA_approval_request();
                m_is_OTA_approval_request_sent = true;
                return_flag = true;
            }
            else if (persisted_state == Utils::PREPARING_0X22)
            {
                LOG_INFO << "Reloaded state is preparing0X22, hence waiting for either stop or finish command "
                         << std::endl;
                m_mqtt_client.reload_campaign_id();
            }
            else if (persisted_state == Utils::PREPARED_WAITING_IG_OFF)
            {
                LOG_INFO << "Reloaded state is preparedWaitingIgOff, hence waiting for IG OFF" << std::endl;
                m_mqtt_client.reload_campaign_id();
            }
            else if (persisted_state == Utils::PREPARED)
            {
                LOG_INFO << "Reloaded state is prepared, hence waiting for either download,stop or finish command " << std::endl;
                m_mqtt_client.reload_campaign_id();
                Honda::Honda_campaign campaign;
                campaign.m_campaign_id = m_mqtt_client.Get_campaign_id();
                m_campaign_map.insert(std::pair<std::string, Honda::Honda_campaign>(campaign.m_campaign_id, campaign));
            }
            else if (persisted_state == Utils::PREPARE_FAILED ||
                     persisted_state == Utils::PREPARE_STOPPED)
            {
                LOG_INFO << "Reloaded state is " << Utils::get_internal_state_name(persisted_state)
                         << ", hence waiting for finish command " << std::endl;
                m_mqtt_client.reload_campaign_id();
            }
            else if ((persisted_state == Utils::DOWNLOAD_REQUESTED) ||
                     (persisted_state == Utils::DOWNLOADING) ||
                     (persisted_state == Utils::DOWNLOADED) ||
                     (persisted_state == Utils::INSTALLING) ||
                     (persisted_state == Utils::INSTALLED) ||
                     (persisted_state == Utils::ACTIVATING) ||
                     (persisted_state == Utils::DOING_ROLLBACK) ||
                     (persisted_state == Utils::ACTIVATED) ||
                     (persisted_state == Utils::ACTIVATED_ASSEMBLY_UPDATED) ||
                     (persisted_state == Utils::FAILED) ||
                     (persisted_state == Utils::FAILED_ASSEMBLY_UPDATED) ||
                     (persisted_state == Utils::STOPPED) ||
                     (persisted_state == Utils::STOPPED_ASSEMBLY_UPDATED))
            {
                LOG_INFO << "Reloaded state is " << Utils::get_internal_state_name(persisted_state)
                         << std::endl;
                m_mqtt_client.reload_campaign_id();
                if (persistency->has(Utils::HONDA_OTA_CMD_DOWNLOAD))
                {
                    std::string reloaded_download_data = persistency->read(Utils::HONDA_OTA_CMD_DOWNLOAD);
                    m_mqtt_client.reload_require_download_data(reloaded_download_data);
                    if (m_mqtt_client.Get_campaign_id().compare(m_mqtt_client.get_Download_request_handler().campaignId) == 0)
                    {
                        LOG_INFO << "Doing campaign mapping" << std::endl;
                        campaign_mapping();
                    }
                }
                else
                    LOG_INFO << "Key HONDA_OTA_CMD_DOWNLOAD not found" << std::endl;
            }
            else if (persisted_state == Utils::FINISHING)
            {
                LOG_INFO << "Reloaded state is " << Utils::get_internal_state_name(persisted_state)
                         << ", hence sending finish OTA request."
                         << std::endl;
                send_VCU_finish_OTA_request();
                m_is_finish_OTA_request_sent = true;
                return_flag = true;
            }
            if (persistency->has(Utils::HONDA_IS_OTA_APPROVED))
                m_is_OTA_approved = (persistency->read(Utils::HONDA_IS_OTA_APPROVED) == "1") ? true : false;
            LOG_INFO << "m_is_OTA_approved = " << m_is_OTA_approved << std::endl;

            return std::make_pair(persisted_state, return_flag);
        }

        // To handle timeout in specific states
        void Job_synchronization_honda_impl::backend_supervision_timer(int8_t &supervision_timer)
        {
            if (supervision_timer == TIMEOUT)
            {
                LOG_INFO << "Did not receive desired command, hence doing autonomous finish" << std::endl;
                supervision_timer = 0;
                m_mqtt_client.set_inform_to_finish_request(true);
            }
            else
                supervision_timer++;
        }

        Job_synchronization::Response
        Job_synchronization_honda_impl::pull(std::string const & /*vehicle_assembly_fingerprint*/)
        {
            LOG_DEBUG << "Job_synchronization_honda_impl::pull() called, on_startup=" << m_on_startup
                      << std::endl;
            try // Separate try block to connect to broker so that it does not block pull method
            {
                if (!m_mqtt_client.is_connected())
                {
                    LOG_INFO << "Broker not connected, connecting...." << std::endl;
                    connect_to_mqtt_broker(m_mqtt_client);
                    std::vector<std::string> topic_list = {m_vehicle_id + "/ota/cmd",
                                                           m_vehicle_id + "/ota/info",
                                                           m_vehicle_id + "/ota/security-info"};
                    m_mqtt_client.subscribe_multiple(topic_list, m_mqtt_qos.first);
                }
            }
            catch (const std::exception &e)
            {
                LOG_INFO << "Could not connect to broker,retrying...." << std::endl;
            }

            try
            {
                // if (m_jwt.empty())
                // {
                //     cc::service::Json_web_token_retrieval jwt_retrieval(m_mqtt_client, m_mqtt_qos, m_stage,
                //                                                         m_vehicle_id);
                //     m_jwt = jwt_retrieval.retrieve_json_web_token();
                // }
                int8_t current_state = m_mqtt_client.get_job_state();

                std::string persisted_state_string = "";

                std::string IG_OFF_path = m_TCU_path + "ignition.off";
                std::string keep_alive_path = m_TCU_path + "keep.alive";

                Poco::File IG_OFF_file(IG_OFF_path);
                Poco::File keep_alive_file(keep_alive_path);
                bool IG_OFF_file_existence = IG_OFF_file.exists();
                m_power_status = (IG_OFF_file_existence) ? "off" : "on";
                auto persistency = m_mqtt_client.get_persistency_service();

                if (m_on_startup) // start up block
                {
                    Utils::set_job_state(current_state, current_state, m_mqtt_client);
                    LOG_INFO << "On startup process begins " << std::endl;
                    LOG_INFO << "IG_OFF_file_existence = " << IG_OFF_file_existence
                             << ", m_power_status = " << m_power_status << std::endl;
                    if ((!m_mqtt_client.is_iginition_status_sent()) &&
                        (m_mqtt_client.is_connected())) // For publishing IG status
                    {
                        Utils::publish_IGON(m_mqtt_client, m_mqtt_qos, m_vehicle_id, m_power_status);
                        m_mqtt_client.set_ignition_status_sent(true);
                    }
                    else
                    {
                        LOG_INFO << "Ignition status not sent" << std::endl;
                    }

                    if (persistency->has(Utils::HONDA_JOB_STATE)) // To reload persisted state
                    {
                        persisted_state_string = persistency->read(Utils::HONDA_JOB_STATE);
                        m_persisted_state = Utils::get_current_state_id(persisted_state_string);
                        LOG_INFO << "Persisted state retrieved = " << persisted_state_string
                                 << std::endl;
                        if (m_persisted_state == Utils::IDLE) // current_state == idle
                        {
                            LOG_INFO << "Current state is idle.Hence, no OTA campaign known" << std::endl;
                            m_persisted_state = INVALID_STATE;
                        }
                        else
                        {
                            Utils::set_job_state(m_persisted_state, current_state, m_mqtt_client);
                            if (m_persisted_state != INVALID_STATE)
                            {
                                std::pair<int8_t, bool> return_pair = persisted_state_handling(m_persisted_state);
                                m_persisted_state = return_pair.first;
                                // if(return_pair.second)
                                // {
                                //     m_persisted_state = INVALID_STATE;
                                //     LOG_INFO<<"Returning here"<<std::endl;
                                //     return Job_synchronization::Response {Job_synchronization::Response::Code::SUCCESS, m_jobs};
                                // }
                                m_persisted_state = INVALID_STATE;
                            }
                        }
                    }
                    else
                        LOG_WARN << "Key HONDA_JOB_STATE does not exist" << std::endl;
                    m_on_startup = false;
                    // On startup, an ASSEMBLY_FINGERPRINT_MISMATCH must be returned, such that cadian will trigger an assembly upload
                    return Job_synchronization::Response{
                        Job_synchronization::Response::Code::ASSEMBLY_FINGERPRINT_MISMATCH, {}};
                }
                if (!m_mqtt_client.is_iginition_status_sent() &&
                    m_mqtt_client.is_connected()) // To send IG status if it was not sent during startup
                {
                    Utils::publish_IGON(m_mqtt_client, m_mqtt_qos, m_vehicle_id, m_power_status);
                    m_mqtt_client.set_ignition_status_sent(true);
                }

                if (m_mqtt_client.is_package_install_failed() != "") // update install result on
                                                                     //  installation as failed (on failed)
                {
                    installResult->update_install_result_for_ecu(m_mqtt_client.is_package_install_failed(), FAILED);
                    m_mqtt_client.reset_package_install();
                }

                if (m_mqtt_client.is_package_install_finished() != "") // update install result on installation
                                                                       // as success (on finish)
                {
                    installResult->update_install_result_for_ecu(m_mqtt_client.is_package_install_finished(), SUCCEEDED);
                    m_mqtt_client.reset_package_install();
                }

                if (m_mqtt_client.is_inform_to_finish_request()) // To process finish command
                {
                    LOG_INFO << "Inform to finish received for current state = "
                             << Utils::get_internal_state_name(current_state)
                             << std::endl;

                    if (current_state == Utils::PREPARED ||
                        current_state == Utils::PREPARED_WAITING_IG_OFF ||
                        current_state == Utils::PREPARE_FAILED ||
                        current_state == Utils::PREPARING_0X22 ||
                        current_state == Utils::PREPARE_STOPPED ||
                        current_state == Utils::ACTIVATED ||
                        current_state == Utils::ACTIVATED_ASSEMBLY_UPDATED ||
                        current_state == Utils::FAILED ||
                        current_state == Utils::FAILED_ASSEMBLY_UPDATED ||
                        current_state == Utils::STOPPED ||
                        current_state == Utils::STOPPED_ASSEMBLY_UPDATED)

                    {
                        if (m_is_OTA_approved && !m_is_finish_OTA_request_sent) // OTA  approved during prepare
                        {
                            send_VCU_finish_OTA_request();
                            Utils::set_job_state(Utils::FINISHING, current_state, m_mqtt_client);
                            m_is_finish_OTA_request_sent = true;
                        }
                        else // OTA not approved during prepare
                        {
                            Utils::set_job_state(Utils::IDLE, current_state, m_mqtt_client);
                        }
                        m_mqtt_client.set_cancel_campaign_sent(false); // For preparedStopped state
                        m_mqtt_client.set_cancel_campaign_completed_sent(false);
                        auto persist_service = m_mqtt_client.get_persistency_service();
                        persist_service->remove(Utils::HONDA_INSTALL_RESULT);
                        persist_service->remove(Utils::HONDA_OTA_CMD_PREPARE);
                        persist_service->remove(Utils::HONDA_OTA_CMD_DOWNLOAD);
                        persist_service->remove(Utils::HONDA_IS_OTA_APPROVED);
                        installResult.reset();
                    }
                    else
                        LOG_WARN << "Finish command ignored in " << Utils::get_internal_state_name(current_state)
                                 << std::endl;
                    m_supervision_timer = 0;
                    m_mqtt_client.set_inform_to_finish_request(false);
                    return Job_synchronization::Response{Job_synchronization::Response::Code::SUCCESS, m_jobs};
                }

                if (m_is_finish_OTA_response_received) // To proceed with removing job and transiting to idle state
                                                       //  if finish OTA response was received from VCU
                {
                    LOG_INFO << "Finish OTA response received" << std::endl;
                    m_mqtt_client.clear_url_map();
                    m_campaign_map.erase(m_mqtt_client.Get_campaign_id());

                    auto job_iterator = std::find(begin(m_jobs), end(m_jobs), m_mqtt_client.Get_campaign_id());
                    if (job_iterator != std::end(m_jobs)) // To remove job
                    {
                        LOG_INFO << "removing job:" << job_iterator->m_id << std::endl;
                        m_jobs.erase(job_iterator);
                    }
                    if (keep_alive_file.exists()) // To delete keep alive file
                    {
                        LOG_INFO << "Deleting keep alive file" << std::endl;
                        keep_alive_file.remove(false);
                    }
                    Utils::set_job_state(Utils::IDLE, current_state, m_mqtt_client);
                    m_is_finish_OTA_response_received = false;
                    m_is_finish_OTA_request_sent = false;
                    return Job_synchronization::Response{Job_synchronization::Response::Code::SUCCESS, m_jobs};
                }

                if (m_mqtt_client.is_vehicle_assembly_request()) // To process vehicle assembly request
                {
                    LOG_INFO << "Vehicle assembly request received for current state = "
                             << Utils::get_internal_state_name(current_state)
                             << std::endl;

                    if (current_state != Utils::FAILED && current_state != Utils::STOPPED && current_state != Utils::ACTIVATED && m_mqtt_client.is_initial_assembly_available()) // when backend sends vehicle assembly request
                    {
                        Utils::publish_vehicle_assembly(m_mqtt_client, m_mqtt_qos, m_vehicle_id);
                        m_mqtt_client.set_vehicle_assembly_request(false);
                        m_supervision_timer = 0;
                    }
                    else if (m_mqtt_client.is_new_assembly_available()) // when vehicle assembly request is sent manually
                    {
                        Utils::publish_vehicle_assembly(m_mqtt_client, m_mqtt_qos, m_vehicle_id);
                        m_mqtt_client.set_vehicle_assembly_request(false);
                    }
                    else // when vehilce assembly is not available
                    {
                        LOG_WARN << "Assembly not available, still collecting" << std::endl;
                    }
                    /**************/
                    return Job_synchronization::Response{Job_synchronization::Response::Code::SUCCESS, m_jobs};
                }

                if ((current_state == Utils::FAILED || current_state == Utils::ACTIVATED || current_state == Utils::STOPPED) && !m_mqtt_client.is_mismatch_sent()) // To collect vehicle assembly in failed,
                                                                                                                                                                   //  stopped and activated state
                {
                    m_mqtt_client.set_vehicle_assembly_request(true);
                    m_mqtt_client.mismatch_sent();
                    return Job_synchronization::Response{Job_synchronization::Response::Code::ASSEMBLY_FINGERPRINT_MISMATCH, {}};
                }
                if (current_state == Utils::FAILED || current_state == Utils::STOPPED) // To publish ota event
                                                                                       // cancel campaign started for failed and stopped states
                {
                    if (m_mqtt_client.is_cancel_campaign_sent() == false &&
                        (!m_mqtt_client.is_cancel_campaign_completed_sent())) // To check whether cancel campaign
                                                                              //  started was already sent or not
                    {
                        LOG_INFO << "Setting the cancel_campaign_sent flag and publish_cancelCampaignStarted : "
                                 << std::to_string(current_state)
                                 << std::endl;

                        // set flag
                        m_mqtt_client.set_cancel_campaign_sent(true);
                        Utils::publish_ota_event(m_mqtt_client,
                                                 m_mqtt_qos,
                                                 m_vehicle_id,
                                                 Utils::get_ota_event_name(Utils::CANCEL_CAMPAIGN_STARTED));
                    }
                    else if (m_mqtt_client.is_new_assembly_available() == true) // To publish ota event cancel campaign
                                                                                // completed for failed and stopped states
                    {
                        LOG_INFO << "resetting the cancel_campaign_sent flag and publish_cancelCampaignCompleted" << std::endl;
                        (current_state == Utils::FAILED) ? (Utils::set_job_state(Utils::FAILED_ASSEMBLY_UPDATED, current_state, m_mqtt_client))
                                                         : (Utils::set_job_state(Utils::STOPPED_ASSEMBLY_UPDATED, current_state, m_mqtt_client));
                        m_mqtt_client.reset_mismatch_sent();
                        LOG_INFO << "current state = " << std::to_string(m_mqtt_client.get_job_state()) << std::endl;
                        Utils::publish_ota_event(m_mqtt_client,
                                                 m_mqtt_qos,
                                                 m_vehicle_id,
                                                 Utils::get_ota_event_name(Utils::CANCEL_CAMPAIGN_COMPLETED));
                        // Removing aborted job
                        LOG_INFO << "Before erase,m_jobs size = " << std::to_string(m_jobs.size()) << std::endl;
                        if (m_jobs.size() > 0)
                            m_jobs.erase(m_jobs.begin());
                        LOG_INFO << "After erase,m_jobs size = " << std::to_string(m_jobs.size()) << std::endl;
                        // reset flag
                        m_mqtt_client.set_cancel_campaign_sent(false);
                        m_mqtt_client.set_cancel_campaign_completed_sent(true);
                    }
                    else
                    { // when assembly is not yet collected
                        LOG_INFO << "new assembly is not ready yet!!!" << std::endl;
                    }
                    return Job_synchronization::Response{Job_synchronization::Response::Code::SUCCESS, m_jobs};
                }

                if (m_mqtt_client.is_request_to_stop_campaign()) // To process stop command
                {
                    LOG_INFO << "Stop command received for current state = "
                             << Utils::get_internal_state_name(current_state)
                             << std::endl;
                    if (current_state == Utils::PREPARING)
                    {
                        LOG_INFO << "Stop command received for preparing state, hence waiting for other"
                                 << " job states to process it." << std::endl;
                        m_mqtt_client.set_stop_in_preparing(true);
                    }
                    else if (current_state == Utils::PREPARED_WAITING_IG_OFF || current_state == Utils::PREPARED || current_state == Utils::PREPARING_0X22)
                    {
                        Utils::publish_ota_event(m_mqtt_client,
                                                 m_mqtt_qos,
                                                 m_vehicle_id,
                                                 Utils::get_ota_event_name(Utils::CANCEL_CAMPAIGN_STARTED));
                        m_mqtt_client.set_cancel_campaign_sent(true);
                    }
                    else if (current_state == Utils::DOWNLOADED)
                        m_mqtt_client.set_stop_in_downloaded(true);
                    else if (current_state == Utils::DOWNLOAD_REQUESTED ||
                             current_state == Utils::DOWNLOADING)
                    {
                        LOG_INFO << "Cancelling campaign...";
                        for (auto &job : m_jobs)
                        {
                            LOG_INFO << "job.m_campaign = " << job.m_campaign
                                     << " m_mqtt_client.Get_campaign_id() = "
                                     << m_mqtt_client.Get_campaign_id() << std::endl;

                            if (job.m_campaign.find(m_mqtt_client.Get_campaign_id()) != std::string::npos)
                            {
                                LOG_INFO << "Setting campaign cancelling" << std::endl;
                                job.m_state = Job_synchronization::Job_state::canceled;
                            }
                        }
                        LOG_INFO << "M_State = " << static_cast<int>(m_jobs[0].m_state) << std::endl;
                    }
                    else // when stop is received for an invalid state
                    {
                        LOG_WARN << "Stop command ignored in " << Utils::get_internal_state_name(current_state)
                                 << std::endl;
                    }
                    m_mqtt_client.reset_request_to_stop_campaign();
                    return Job_synchronization::Response{Job_synchronization::Response::Code::SUCCESS, m_jobs};
                }
                if ((m_mqtt_client.is_cancel_campaign_sent() &&
                     (!m_mqtt_client.is_cancel_campaign_completed_sent()))) // To send cancel campaign completed
                                                                            // when cancel campaign started is already sent
                {
                    LOG_INFO << "Cancel campaign started sent, now sending cancel campaign completed."
                             << std::endl;
                    if ((current_state == Utils::PREPARED_WAITING_IG_OFF) ||
                        (current_state == Utils::PREPARED) ||
                        (current_state == Utils::PREPARING_0X22))
                    {
                        Utils::set_job_state(Utils::PREPARE_STOPPED, current_state, m_mqtt_client);
                        m_supervision_timer = 0;
                        Utils::publish_ota_event(m_mqtt_client,
                                                 m_mqtt_qos,
                                                 m_vehicle_id,
                                                 Utils::get_ota_event_name(Utils::CANCEL_CAMPAIGN_COMPLETED));
                    }
                    else if (current_state == Utils::PREPARE_FAILED)
                        Utils::publish_ota_event(m_mqtt_client,
                                                 m_mqtt_qos,
                                                 m_vehicle_id,
                                                 Utils::get_ota_event_name(Utils::CANCEL_CAMPAIGN_COMPLETED));
                    m_mqtt_client.set_cancel_campaign_sent(false);
                    m_mqtt_client.set_cancel_campaign_completed_sent(true);
                    return Job_synchronization::Response{Job_synchronization::Response::Code::SUCCESS, m_jobs};
                }

                if (m_mqtt_client.is_require_progress_request()) // To process progress command
                {
                    LOG_INFO << "Progress report request received for current state = "
                             << Utils::get_internal_state_name(current_state)
                             << std::endl;
                    m_supervision_timer = 0;
                    uint32_t progress_percent = Utils::get_progress_percent(current_state, m_status_report);
                    std ::cout << "line no 347" << std::endl;
                    Utils::publish_progress_report(m_mqtt_client, m_mqtt_qos, m_vehicle_id, progress_percent);
                    std ::cout << "line no 349" << std::endl;
                    m_mqtt_client.set_progress_report_request(false);
                    /**************/
                    return Job_synchronization::Response{Job_synchronization::Response::Code::SUCCESS, m_jobs};
                }
                if (m_mqtt_client.is_keyinfo_request()) // To process key info command
                {
                    if (m_key_delay_flag)
                    {
                        LOG_INFO << "Processing Key info request for current state = "
                                 << Utils::get_internal_state_name(current_state)
                                 << std::endl;
                        Utils::publish_keyinfo(m_mqtt_client, m_mqtt_qos, m_vehicle_id);
                        m_mqtt_client.set_keyinfo_request(false);
                        m_key_delay_flag = false;
                    }
                    else
                    {
                        LOG_INFO << "Key info request received for current state = "
                                 << Utils::get_internal_state_name(current_state)
                                 << ", will be processed in next cycle."
                                 << std::endl;
                        m_key_delay_flag = true;
                    }
                    return Job_synchronization::Response{Job_synchronization::Response::Code::SUCCESS, m_jobs};
                }
                if (m_mqtt_client.is_signature_request()) // To process signature command
                {
                    if (m_sign_delay_flag)
                    {
                        LOG_INFO << "Signature request received for current state = "
                                 << Utils::get_internal_state_name(current_state)
                                 << std::endl;
                        Utils::publish_signature(m_mqtt_client, m_mqtt_qos, m_vehicle_id);
                        m_mqtt_client.set_signature_request(false);
                        m_sign_delay_flag = false;
                    }
                    else
                    {
                        LOG_INFO << "Signature request received for current state = "
                                 << Utils::get_internal_state_name(current_state)
                                 << std::endl;
                        m_sign_delay_flag = true;
                    }
                    return Job_synchronization::Response{Job_synchronization::Response::Code::SUCCESS, m_jobs};
                }

                if (m_mqtt_client.is_prepare_campaign_request()) // To process prepare command
                {
                    LOG_INFO << "Prepare command received for current state = "
                             << Utils::get_internal_state_name(current_state)
                             << std::endl;

                    // delete download_path
                    if (std::experimental::filesystem::exists(m_download_path + "/packages"))
                    {
                        if (std::experimental::filesystem::remove_all(m_download_path + "/packages") != 0)
                        {
                            LOG_INFO << "Deleted " << m_download_path << "/packages" << std::endl;
                        }
                    }

                    if (!m_is_OTA_approval_request_sent) // When OTA approval request is not sent before
                    {
                        Utils::set_job_state(Utils::PREPARING, current_state, m_mqtt_client);
                        send_VCU_OTA_approval_request();
                        LOG_INFO << "OTA approval request triggered from Pull" << std::endl;
                        m_is_OTA_approval_request_sent = true;
                    }
                    else // OTA approval request already sent
                    {
                        LOG_INFO << "OTA approval request already sent to VCU" << std::endl;
                    }
                    m_mqtt_client.set_prepare_campaign_request(false);
                    /**************/
                    return Job_synchronization::Response{Job_synchronization::Response::Code::SUCCESS, m_jobs};
                }
                if ((!m_is_OTA_approval_response_received) && (current_state == Utils::PREPARING)) // OTA approval
                                                                                                   // response not received from VCU
                {
                    LOG_WARN << "OTA approval response not received from VCU, hence not "
                             << "proceeding with prepare command" << std::endl;
                    m_is_OTA_approval_request_sent = false;
                }
                if ((m_is_OTA_approval_response_received) && (current_state == Utils::PREPARING)) // VCU response received = true
                                                                                                  // and current state = preparing
                {
                    LOG_INFO << "OTA campaign approval response received for current state = "
                             << Utils::get_internal_state_name(current_state) << std::endl;
                    std::string IG_OFF_required = "yes";
                    std::string error_code;

                    if (m_is_OTA_approved) // VCU approves OTA request i.e positive response
                    {
                        if (m_is_IG_OFF_required) // IG-OFF required by VCU = true
                        {
                            LOG_INFO << "IG OFF is required by VCU hence creating keep.alive file" << std::endl;
                            try
                            {
                                bool keep_alive_created = keep_alive_file.createFile();
                                if (!keep_alive_created)
                                {
                                    LOG_WARN << "Keep alive file not created as it already exists" << std::endl;
                                }
                                Utils::set_job_state(Utils::PREPARED_WAITING_IG_OFF, current_state, m_mqtt_client);
                            }
                            catch (const std::exception &e)
                            {
                                LOG_INFO << "catch entered." << std::endl;
                                Utils::set_job_state(Utils::PREPARE_FAILED, current_state, m_mqtt_client);
                                Utils::set_error_code(KEEP_ALIVE_ERROR_CODE);
                                // set ota_status == campaignCanceling and error code == 02000
                            }
                        }
                        else // IG-OFF NOT required by VCU
                        {
                            LOG_INFO << "IG OFF not required by VCU" << std::endl;
                            IG_OFF_required = "no";
                            Utils::set_job_state(Utils::PREPARED, current_state, m_mqtt_client);
                            /********************************************/
                            Honda::Honda_campaign campaign;
                            campaign.m_campaign_id = m_mqtt_client.Get_campaign_id();
                            m_campaign_map.insert(std::pair<std::string, Honda::Honda_campaign>(campaign.m_campaign_id, campaign));
                            /********************************************/
                        }
                    }
                    else if (m_nrc_value == CONDITION_NOT_CORRECT) // OTA request not approved by VCU i.e negative
                                                                   //  response and nrc = 0X22(cond. not correct)
                    {
                        LOG_INFO << "Condition not correct." << std::endl;
                        Utils::set_job_state(Utils::PREPARING_0X22, current_state, m_mqtt_client);
                    }
                    else // OTA request not approved by VCU i.e. neagtive response nrc != 0X22(cond. not correct)
                    {
                        LOG_INFO << "OTA NOT approved for current_state = "
                                 << Utils::get_internal_state_name(current_state)
                                 << ", nrc value = " << std::to_string(static_cast<int8_t>(m_nrc_value))
                                 << std::endl;
                        Utils::set_job_state(Utils::PREPARE_FAILED, current_state, m_mqtt_client);
                        Utils::set_error_code(NRC_ERROR_CODE);
                    }
                    persistency->write(Utils::HONDA_IS_OTA_APPROVED, std::to_string(m_is_OTA_approved));
                    LOG_INFO << "Publishing preparation complete" << std::endl;
                    Utils::publish_preparation_complete(m_mqtt_client, m_mqtt_qos, m_vehicle_id, IG_OFF_required);
                    m_is_OTA_approval_request_sent = false;
                    return Job_synchronization::Response{Job_synchronization::Response::Code::SUCCESS, m_jobs};
                }
                if (m_mqtt_client.is_request_to_stop_in_preparing()) // when stop was received during preparing state
                {
                    LOG_INFO << "Stop was received during preparing state, hence now processing it for "
                             << Utils::get_internal_state_name(current_state)
                             << std::endl;
                    if (current_state == Utils::PREPARE_FAILED)
                    {
                        LOG_WARN << "Stop command ignored in prepare failed state" << std::endl;
                    }
                    else // For states other than prepare_failed
                    {
                        Utils::publish_ota_event(m_mqtt_client,
                                                 m_mqtt_qos,
                                                 m_vehicle_id,
                                                 Utils::get_ota_event_name(Utils::CANCEL_CAMPAIGN_STARTED));
                        m_mqtt_client.set_cancel_campaign_sent(true);
                    }
                    m_mqtt_client.set_stop_in_preparing(false);
                }

                if (IG_OFF_file_existence &&
                    (current_state == Utils::PREPARED_WAITING_IG_OFF) &&
                    (!m_mqtt_client.is_cancel_campaign_sent() /*for stop during preparing state*/))
                // To publish power status for PREPARED_WAITING_IG_OFF state if ignition.off file exists
                {
                    LOG_INFO << "IG_OFF_file_existence = " << IG_OFF_file_existence
                             << ", m_power_status = " << m_power_status << std::endl;
                    Utils::publish_IGON(m_mqtt_client, m_mqtt_qos, m_vehicle_id, m_power_status);
                    Utils::set_job_state(Utils::PREPARED, current_state, m_mqtt_client);
                    /********************************************/
                    Honda::Honda_campaign campaign;
                    campaign.m_campaign_id = m_mqtt_client.Get_campaign_id();
                    m_campaign_map.insert(std::pair<std::string, Honda::Honda_campaign>(campaign.m_campaign_id, campaign));
                    /********************************************/
                }
                if ((current_state == Utils::PREPARE_FAILED) &&
                    (!m_mqtt_client.is_cancel_campaign_sent()) &&
                    (!m_mqtt_client.is_cancel_campaign_completed_sent()))
                // To publish ota event cancel campaign started in prepare_failed state
                {
                    Utils::publish_ota_event(m_mqtt_client,
                                             m_mqtt_qos,
                                             m_vehicle_id,
                                             Utils::get_ota_event_name(Utils::CANCEL_CAMPAIGN_STARTED));
                    m_mqtt_client.set_cancel_campaign_sent(true);
                    return Job_synchronization::Response{Job_synchronization::Response::Code::SUCCESS, m_jobs};
                }

                if (m_mqtt_client.is_download_request()) // To process download command
                {
                    LOG_INFO << "Download command received for current state =  "
                             << Utils::get_internal_state_name(current_state)
                             << std::endl;
                    if (current_state == Utils::PREPARED)
                    {
                        //  bool find(const KEY& key_value, std::pair<KEY, VALUE>& pair_result)char
                        std::pair<std::string, cc::service::Honda::Honda_campaign> campaign;
                        if (!m_campaign_map.find(m_mqtt_client.get_Download_request_handler().campaignId, campaign))
                        {
                            return Job_synchronization::Response{Job_synchronization::Response::Code::SUCCESS, m_jobs};
                        }

                        campaign.second.m_vehicle_id = m_mqtt_client.get_Download_request_handler().frame_type_and_number;
                        campaign.second.m_campaign_id = m_mqtt_client.get_Download_request_handler().campaignId;
                        campaign.second.m_url_overwritten = false;
                        campaign.second.m_expected_apply_time = 60;
                        for (const auto &ecu : m_mqtt_client.get_Download_request_handler().ecu_list)
                        {
                            if (ecu.downloadUrlNew.empty())
                            {
                                LOG_DEBUG << "No Update package url received for :" << ecu.ecuTypeId << std::endl;
                                continue;
                            }
                            campaign.second.m_ecus.push_back({std::to_string(std::stoi(ecu.physicalTargetId, nullptr, 16)),
                                                              ecu.canEther,
                                                              ecu.newSwNo,
                                                              ecu.oldSwNo,
                                                              ecu.downloadUrlNew,
                                                              0,
                                                              ecu.downloadUrlOld,
                                                              0,
                                                              ecu.downloadUrlBoot,
                                                              0,
                                                              ecu.softwareVersionNew,
                                                              ecu.softwareVersionOld});
                            m_mqtt_client.insert_to_map({std::to_string(std::stoi(ecu.physicalTargetId, nullptr, 16)) + "new", {ecu.downloadUrlNew, ecu.hashNew}});
                            m_mqtt_client.insert_to_map({std::to_string(std::stoi(ecu.physicalTargetId, nullptr, 16)) + "rollback", {ecu.downloadUrlOld, ecu.hashOld}});
                            // m_mqtt_client.insert_to_map({std::to_string(std::stoi( ecu.physicalTargetId, nullptr, 16))+"rollback", {ecu.downloadUrlBoot,ecu.hashBoot}});
                        }
                        campaign.second.m_timing_setting = {"immediate", "0011201202", "immediate", "1011201202"};
                        campaign.second.m_campaign_condition = {{campaign.second.m_campaign_id}, {}, campaign.second.m_ecus};

                        installResult = std::make_shared<Utils::InstallResult>(campaign.second.m_ecus,
                                                                               m_mqtt_client.get_Download_request_handler().ecu_list,
                                                                               m_mqtt_client.get_persistency_service());
                        LOG_INFO << "Install result created: " << installResult->to_string() << std::endl;

                        LOG_INFO << "Job sync Honda Campaign before map insert " << campaign.second << std::endl;

                        m_campaign_map.emplace(std::pair<std::string, Honda::Honda_campaign>(campaign.second.m_campaign_id, campaign.second));
                        m_jobs.push_back({campaign.second.m_vehicle_id,
                                          campaign.second.m_campaign_id,
                                          "ebri:eb:honda.local:campaign-service:campaign:campaigns/" + campaign.second.m_campaign_id,
                                          "ebri:eb:local:honda:update-execution-plan:actions/" + campaign.second.m_campaign_id,
                                          Job_synchronization::Job_state::pending,
                                          "null"});
                        m_mqtt_client.set_new_assembly_available(false);

                        std::pair<std::string, cc::service::Honda::Honda_campaign> campaign1;
                        if (!m_campaign_map.find(m_mqtt_client.get_Download_request_handler().campaignId, campaign1))
                        {
                            LOG_INFO << "Job sync find is null " << std::endl;
                        }
                        LOG_INFO << "Job sync Honda Campaign after map insert " << campaign1.second << std::endl;
                        Utils::set_job_state(Utils::DOWNLOAD_REQUESTED, current_state, m_mqtt_client);
                        m_supervision_timer = 0;
                    }
                    else // when download command is received for state != prepared state
                    {
                        LOG_INFO << "Download command ignored as current state is not prepared" << std::endl;
                    }
                    m_mqtt_client.set_download_request(false);
                    /**************/
                    return Job_synchronization::Response{Job_synchronization::Response::Code::SUCCESS, m_jobs};
                }
                if ((current_state != Utils::IDLE) &&
                    (current_state != Utils::PREPARING) &&
                    (current_state != Utils::PREPARED_WAITING_IG_OFF) &&
                    (current_state != Utils::DOWNLOAD_REQUESTED) &&
                    (current_state != Utils::DOWNLOADING) &&
                    (current_state != Utils::DOWNLOADED) &&
                    (current_state != Utils::INSTALLING) &&
                    (current_state != Utils::INSTALLED) &&
                    (current_state != Utils::ACTIVATING) &&
                    (current_state != Utils::DOING_ROLLBACK) &&
                    (current_state != Utils::FINISHING))
                // To start supervision timer for states other than the ones mentioned above
                {
                    LOG_INFO << "Starting supervision timer for current state = "
                             << Utils::get_internal_state_name(current_state) << std::endl;
                    backend_supervision_timer(m_supervision_timer);
                }

                return Job_synchronization::Response{Job_synchronization::Response::Code::SUCCESS, m_jobs};
            }
            catch (Job_synchronization::Exception const &)
            {
                throw;
            }
            // any exceptions from lower layers are caught and converted into Job_control_exceptions
            catch (std::exception const &e)
            {
                LOG_THROW(Job_synchronization::Exception, e.what())
            }
        }

        void Job_synchronization_honda_impl::push(std::string const & /*vehicle_assembly_fingerprint*/,
                                                  Job_synchronization::Jobs const &jobs)
        {
            LOG_DEBUG << "Job_synchronization_honda_impl::push() called" << std::endl;
            // if (m_jwt.empty())
            // {
            //     m_jwt = retrieve_json_web_token(m_mqtt_client, m_mqtt_qos, m_stage, m_vehicle_id);
            // }
            // // temporarily store the list of campaigns that can be reported as finished so that we always send
            // // the full list of finished/failed campaigns to the Honda Backend. Otherwise this would reset the
            // // Honda Backend to re-deliver already finished or failed campaigns
            // static std::vector<std::string> campaign_ids_to_be_reported_finished;
            m_jobs = jobs;
            int8_t current_state = m_mqtt_client.get_job_state();
            for (auto const &job : jobs)
            {
                LOG_INFO << "job state: " << static_cast<uint32_t>(job.m_state)
                         << ", current state = "
                         << Utils::get_internal_state_name(current_state)
                         << std::endl;
                if (job.m_state == Job_synchronization::Job_state::failed)
                {
                    // TODO , before sending activateFailed to backend.
                    //  please check the current state should not be downloadfailed.
                    //  as download failed is already sending the OTA event to backend.
                    // list of state that can be send the ota event
                    //  installing
                    //  activating
                    //  doing_rollback
                    if (current_state == Utils::INSTALLING)
                    {
                        LOG_INFO << "Job failed by cadian and current state = "
                                 << Utils::get_internal_state_name(current_state)
                                 << std::endl;
                        Utils::set_job_state(Utils::FAILED, current_state, m_mqtt_client);
                        Utils::set_failed_ota_event(Utils::INSTALL_FAILED);
                        Utils::publish_ota_event(m_mqtt_client,
                                                 m_mqtt_qos,
                                                 m_vehicle_id,
                                                 Utils::get_ota_event_name(Utils::INSTALL_FAILED));
                    }
                    else if (current_state == Utils::ACTIVATING)
                    {
                        LOG_INFO << "Job failed by cadian and current state = "
                                 << Utils::get_internal_state_name(current_state)
                                 << std::endl;
                        Utils::set_job_state(Utils::FAILED, current_state, m_mqtt_client);
                        Utils::set_failed_ota_event(Utils::ACTIVATE_FAILED);
                        Utils::publish_ota_event(m_mqtt_client,
                                                 m_mqtt_qos,
                                                 m_vehicle_id,
                                                 Utils::get_ota_event_name(Utils::ACTIVATE_FAILED));
                    }
                    else if (current_state == Utils::DOING_ROLLBACK)
                    {
                        // Since it is not possible to check rollback failed or
                        // completed, hence sending rollback completed by default
                        LOG_INFO << "Job failed by cadian and current state = "
                                 << Utils::get_internal_state_name(current_state)
                                 << std::endl;
                        Utils::publish_ota_event(m_mqtt_client,
                                                 m_mqtt_qos,
                                                 m_vehicle_id,
                                                 Utils::get_ota_event_name(Utils::ROLLBACK_COMPLETED));
                        Utils::set_job_state(Utils::FAILED, current_state, m_mqtt_client);
                    }
                }
                else if (job.m_state == Job_synchronization::Job_state::aborted)
                {
                    LOG_INFO << "Job aborted by cadian hence setting internal state to stopped" << std::endl;
                    Utils::set_job_state(Utils::STOPPED, current_state, m_mqtt_client);
                }
            }
        }

    } // namespace service
} // namespace cc
