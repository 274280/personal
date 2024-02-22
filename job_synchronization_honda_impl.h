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
#ifndef CC_SERVICE_JOB_CONTROL_HONDA_H
#define CC_SERVICE_JOB_CONTROL_HONDA_H

#include "Service/honda_campaign.h"
#include "connectivity_interfaces/service/job_synchronization.h"
#include "Service/status_report_honda_impl.h"
#include "Service/utils.h"

#include <atomic>
#include <memory>

namespace util
{
    namespace microservice
    {
        template <typename T>
        class Proxy;
    }
} // namespace util

namespace cadian
{
    namespace UDS
    {
        struct UdsClientService;
    }
} // namespace cadian

namespace cadian
{
    namespace UDS
    {
        using PDU = std::vector<uint8_t>;
    }
} // namespace cadian

namespace cadian
{
    namespace connectivity
    {
        class Http_interface;
        class Mqtt_interface;
        class Status_report;
    } // namespace connectivity
} // namespace cadian

namespace cc
{

    template <typename K, typename V>
    class Thread_safe_map;

    namespace service
    {

        class Json_web_token_retrieval;

        /// \brief implements Job_control_service
        ///
        /// The implementation uses protocol::Http_interface for realizing the actual up- and downloads,
        /// the code inside this class handles creating the request parameters and evaluating
        /// the response.
        class Job_synchronization_honda_impl : public cadian::connectivity::Job_synchronization
        {
        public:
            /// \brief constructor
            /// \param http Http_interface to be used for connectivity
            /// \param mqtt Mqtt_interface to be used for JWT retrieval
            /// \param mqtt_qos Quality of Service for MQTT; first is for the subscription, second for publish
            /// \param status_report Status_report interface to be used for storing the campaign status and download status
            /// \param end_point the url of the server where to send the data to
            /// \param stage the stage of the server to use for the connection
            /// \param vehicle_id identifier of the vehicle to be sent together with the requests
            /// \param campaign_map Map that stores the Honda campaigns
            Job_synchronization_honda_impl(
                cadian::connectivity::Http_interface &http,
                cadian::connectivity::Mqtt_interface &mqtt,
                cadian::connectivity::Status_report &status_report,
                std::pair<int32_t, int32_t> mqtt_qos,
                std::string vehicle_id,
                Thread_safe_map<std::string, Honda::Honda_campaign> &campaign_map,
                std::string TCU_path,
                util::microservice::Proxy<cadian::UDS::UdsClientService> const &dsClient_proxy,
                uint16_t uds_client_id,
                uint16_t vcu_id,
                std::string download_path);

            Job_synchronization::Response pull(std::string const &vehicle_assembly_fingerprint) override;

            void push(std::string const &vehicle_assembly_fingerprint,
                      cadian::connectivity::Job_synchronization::Jobs const &jobs) override;

            std::pair<int8_t, bool> persisted_state_handling(int8_t persisted_state);

            void campaign_mapping(); ///< Maps campaign data with job list

            void backend_supervision_timer(int8_t &m_supervision_timer); ///< Starts timer to check whether
                                                                         /// a desired state is achieved or not

        private:
            cadian::connectivity::Http_interface &
                m_http_client;                                    ///< The http client to use for connectivity
            cadian::connectivity::Mqtt_interface &m_mqtt_client;  ///< mqtt client used for JWT retrieval
            cadian::connectivity::Status_report &m_status_report; /// status report to store the status of campaign
            std::pair<uint32_t, uint32_t> m_mqtt_qos;             ///< Quality of service for MQTT
            std::string m_jwt;                                    ///< JSON Web Token
            std::string m_end_point;                              ///< The url of the server where to send the data to
            std::string m_vehicle_id;                             ///< Identifier of the vehicle to be sent together with the requests

            Thread_safe_map<std::string, Honda::Honda_campaign> &m_campaign_map; ///< Honda campaign map
            std::string m_power_status;                                          ///< TCU power status to be published
            std::atomic<bool> m_on_startup;                                      ///< Bool to be used for start up process only
            std::string m_TCU_path;                                              ///< Path of ignition.off file whose existence indicates IG-OFF
                                                                                 /// and vice versa

            cadian::connectivity::Job_synchronization::Jobs m_jobs;
            util::microservice::Proxy<cadian::UDS::UdsClientService> const &m_udsClient_proxy; ///< Proxy for
                                                                                               /// UDS client
            uint16_t m_uds_client_id;
            uint16_t m_vcu_id;
            std::string m_download_path;
            uint8_t m_nrc_value;                                        ///< Negative return code  by VCU
            std::atomic<bool> m_is_OTA_approval_request_sent;           ///< Checks whether OTA campaign approval
                                                                        /// request is sent to VCU or not
            std::atomic<bool> m_is_OTA_approval_response_received;      ///< Set when response is received from VCU
            std::atomic<bool> m_is_OTA_approved;                        ///< Set when VCU approves OTA campaign request
            std::atomic<bool> m_is_IG_OFF_required;                     ///< Set when IG OFF is required by VCU
            std::atomic<bool> m_is_finish_OTA_request_sent;             ///< Checks whether finish OTA campaign approval
                                                                        /// request is sent to VCU or not
            std::atomic<bool> m_is_finish_OTA_response_received;        ///< Set when response is received from VCU
            std::atomic<bool> m_is_finish_OTA_approved;                 ///< Set when VCU approves finish OTA campaign request
            std::shared_ptr<utils::Utils::InstallResult> installResult; ///< Install result for ecus
            const std::string FAILED = "failed";
            const std::string SUCCEEDED = "succeeded";
            int8_t m_supervision_timer;
            int8_t m_persisted_state;
            bool m_key_delay_flag = false;
            bool m_sign_delay_flag = false;

            /*****private methods*********/
            // For OTA approval request
            void send_VCU_OTA_approval_request();                             ///< Sends request to VCU for OTA campaign approval
            void OTA_campaign_approved(cadian::UDS::PDU const &response);     ///< Captures VCU positive respsonse when
                                                                              ///< campaign is approved
            void OTA_campaign_not_approved(cadian::UDS::PDU const &response); ///< Captures VCU negative respsonse
                                                                              ///<  when campaign is not approved
            // For finish OTA request
            void send_VCU_finish_OTA_request();                             ///< Sends request to VCU to finish OTA campaign
            void finish_OTA_approved(cadian::UDS::PDU const &response);     ///< Captures VCU positive respsonse when
                                                                            ///< finish OTA request is approved
            void finish_OTA_not_approved(cadian::UDS::PDU const &response); ///< Captures VCU negative respsonse
                                                                            ///< when finish OTA request is not
                                                                            ///< approved
        };

    } // namespace service
} // namespace cc

#endif
