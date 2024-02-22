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
#include "connectivity_interfaces/protocol/http_interface_mock.h"
#include "connectivity_interfaces/protocol/mqtt_interface_mock.h"
#include <TestHelpers/GTestWrapper.h>
#include "Service/thread_safe_map.h"
#include "Service/utils.h"
#include <MicroService/microservice_proxy.h>
#include <persistency_plugin_interface_mocks/persistency_mock.h>
#include "connectivity_interfaces/service/status_report_mock.h"
#include <UdsClientServiceInterfacesMocks/UdsClientServiceMock.h>
#include <UdsClientServiceInterfaces/UdsClientService.h>
#include <UdsFrames/Routine/UdsRoutineControlRequest.h>
#include <UdsFrames/Routine/UdsRoutineControlResponse.h>
#include "Service/honda_campaign.h"
#include <experimental/filesystem>
#include<vector>
#include <memory>
#include <future>
using ::testing::Return;
using ::testing::Invoke;
namespace cc {

  
inline std::future<void> Async(const std::function<void()>& asyncFunction)
{
    return std::async(std::launch::async, [asyncFunction]() {
        asyncFunction();
    });
}
std::string vehicle_id = "Honda:id";
std::string s1("honda");
std::string campaign_id = "9449448784";
static const std::string fingerprint = "test_fingerprint";
struct cc::service::Honda::Honda_campaign::Ecu ecu1 = {"ecu1_id","ecu_type:1","N:two","O:one","binary_url0",0,"rollback_url0",0,"bootloader_url0",0,"new_ver:15","Old_ver:14"};
struct cc::service::Honda::Honda_campaign::Ecu ecu2 = {"ecu2_id","ecu_type:2","N:four","O:three","binary_url1",0,"rollback_url1",0,"bootloader_url1",0,"new_ver:15","Old_ver:14"};
struct cc::service::Honda::Honda_campaign::Ecu ecu3 = {"ecu3_id","ecu_type:3","N:six","O:five","binary_url2",0,"rollback_url2",0,"bootloader_url2",0,"new_ver:15","Old_ver:14"};
struct cc::service::Honda::Honda_campaign::Timing_setting ts = {"download_timing_setting","download_start_designation","install_timimg_setting","install_start_designation"};

cadian::connectivity::Mqtt_interface::Download_request::Ecu ecu4= {"ecu1_id","can1","20","",16,"downloadUrlBoot0","N:two","hash0","new_ver:15","downloadUrlNew0","hashnew0","Old_ver:14","downloadUrlold","O:one","hasold0"};
cadian::connectivity::Mqtt_interface::Download_request::Ecu ecu5 = {"ecu2_id","can2","20","",16,"downloadUrlBoot1","N:four","hash1","new_ver:15","downloadUrlNew1","hashnew1","Old_ver:14","downloadUrlold","O:three","hasold1"};
cadian::connectivity::Mqtt_interface::Download_request::Ecu ecu6 = {"ecu2_id","can3","20","",16,"downloadUrlBoot3","N:six","hash2","new_ver:15","downloadUrlNew2","hashnew2","Old_ver:14","downloadUrlold","O:five","hasold2"};
cadian::connectivity::Mqtt_interface::Download_request::Ecu ecu7= {"ecu1_id","can1","20","",16,"downloadUrlBoot0","N:two","hash0"," ","","","Old_ver:14"," ","O:one","hasold0"};
std::vector<cc::service::Honda::Honda_campaign::Ecu> m_ecus = {ecu1,ecu2,ecu3};
std::vector<std::string> sv = {"hon1","hon2"};
std::vector<std::string> sv1 = {"hon3","hon4"};
struct cc::service::Honda::Honda_campaign::Campaign_condition condition = {sv,sv1,m_ecus};
struct cc::service::Honda::Honda_campaign  campaign1= {vehicle_id,campaign_id,false,60,m_ecus,ts,condition};
std::string tcu_path = "/tmp/packages/210";
 

// !LINKSTO swdd.connectivityclient.unit_testing, 1
class Job_synchronization_test : public ::testing::Test
{
public:
    void SetUp() override
    {
       m_persistency = std::make_shared<cadian::plugin::Persistency_mock>();
       m_persistency_stub = std::make_shared<util::microservice::Proxy_stub<cadian::plugin::Persistency>>(
                m_persistency);
        m_job_control =
            std::make_unique<cc::service::Job_synchronization_honda_impl>(m_http,m_mqtt,status,m_mqtt_qos,vehicle_id,campaign_map,tcu_path,*uds_proxy,uds_client_id,vcu_id,download_path);
            campaign_map.insert(std::make_pair(campaign_id,campaign1));
    }
      template <typename T>
void EXPECT_pull_throw_impl(T const& exception) {
    EXPECT_CALL(m_mqtt,connect()).WillOnce (testing::Throw(exception));
}
template <typename T>
void EXPECT_pull_throw_impl_(T const& exception) {
    std::string topic = "Honda:id/ota/stat";
    EXPECT_CALL(m_mqtt,publish(topic,testing::_,m_mqtt_qos.second)).WillOnce (testing::Throw(exception));
}
    void TearDown() override { m_job_control.reset(); }

protected:

    cadian::connectivity::Http_interface_mock m_http;
    cadian::connectivity::Mqtt_interface_mock m_mqtt;
    cadian::connectivity::Status_report_mock status;
    cc::Thread_safe_map<std::string, cc::service::Honda::Honda_campaign> campaign_map;
    std::pair<int32_t, int32_t> m_mqtt_qos;
    std::shared_ptr<UdsClientServiceMock> uds_client{std::make_shared<UdsClientServiceMock>()};
    std::shared_ptr<util::microservice::Proxy<cadian::UDS::UdsClientService>> uds_proxy =
    std::make_shared<util::microservice::Proxy_stub<cadian::UDS::UdsClientService>>(uds_client);
    std::unique_ptr<cadian::connectivity::Job_synchronization> m_job_control;
    std::shared_ptr<cadian::plugin::Persistency_mock> m_persistency;
    std::shared_ptr<util::microservice::Proxy_stub<cadian::plugin::Persistency>> m_persistency_stub;
    uint16_t uds_client_id;
    uint16_t vcu_id;
    std::string download_path;
};

TEST_F(Job_synchronization_test,FILE_EXISTENCE) {
std::experimental::filesystem::create_directories("/tmp/packages/210ignition.off");
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_cancel_campaign_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARED_WAITING_IG_OFF));
m_job_control->pull(fingerprint);
m_job_control->pull(fingerprint);
std::experimental::filesystem::remove_all("tmp/packages");
}

TEST_F(Job_synchronization_test,FINGERPRINT_MISMATCH) {
std::string str = "idle";
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillOnce(Return(false)).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillOnce(Return(false));
EXPECT_CALL(m_mqtt,set_ignition_status_sent(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(true));
EXPECT_CALL(*m_persistency,read(testing::_)).WillRepeatedly(Return(str));
EXPECT_pull_throw_impl(std::exception());
auto response =  m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::ASSEMBLY_FINGERPRINT_MISMATCH,response.code);
}

TEST_F(Job_synchronization_test,persisted_state_handling_for_prepared_state) {
std::string str =  "prepared";
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,get_job_state()).WillOnce(Return(cc::service::utils::Utils::INTERNAL_STATES::IDLE));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(true));
EXPECT_CALL(*m_persistency,read(testing::_)).WillRepeatedly(Return(str));
EXPECT_CALL(m_mqtt,Get_campaign_id()).WillRepeatedly(Return("ebri:eb:honda.local:campaign-service:campaign:campaigns/"+ campaign_id));
auto response =  m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::ASSEMBLY_FINGERPRINT_MISMATCH,response.code);
}

TEST_F(Job_synchronization_test,persisted_state_handling_for_prepared_Waiting_IgOff_state) {
std::string str =  "preparedWaitingIgOff";
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,get_job_state()).WillOnce(Return(cc::service::utils::Utils::INTERNAL_STATES::IDLE));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(true));
EXPECT_CALL(*m_persistency,read(testing::_)).WillRepeatedly(Return(str));
EXPECT_CALL(m_mqtt,Get_campaign_id()).WillRepeatedly(Return("ebri:eb:honda.local:campaign-service:campaign:campaigns/"+ campaign_id));
auto response =  m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::ASSEMBLY_FINGERPRINT_MISMATCH,response.code);
}

TEST_F(Job_synchronization_test,persisted_state_handling_for_preparing_state) {
std::string str =  "preparing";
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,get_job_state()).WillOnce(Return(cc::service::utils::Utils::INTERNAL_STATES::IDLE));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(true));
EXPECT_CALL(*m_persistency,read(testing::_)).WillRepeatedly(Return(str));
EXPECT_CALL(m_mqtt,Get_campaign_id()).WillRepeatedly(Return("ebri:eb:honda.local:campaign-service:campaign:campaigns/"+ campaign_id));
auto response =  m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::ASSEMBLY_FINGERPRINT_MISMATCH,response.code);
}

TEST_F(Job_synchronization_test,persisted_state_handling_for_PREPARING_0X22_state) {
std::string str =  "preparing0X22";
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,get_job_state()).WillOnce(Return(cc::service::utils::Utils::INTERNAL_STATES::IDLE));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(true));
EXPECT_CALL(*m_persistency,read(testing::_)).WillRepeatedly(Return(str));
EXPECT_CALL(m_mqtt,Get_campaign_id()).WillRepeatedly(Return("ebri:eb:honda.local:campaign-service:campaign:campaigns/"+ campaign_id));
auto response =  m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::ASSEMBLY_FINGERPRINT_MISMATCH,response.code);
}

TEST_F(Job_synchronization_test,persisted_state_handling_for_prepare_failed_state) {
std::string str =  "prepareFailed";
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,get_job_state()).WillOnce(Return(cc::service::utils::Utils::INTERNAL_STATES::IDLE));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(true));
EXPECT_CALL(*m_persistency,read(testing::_)).WillRepeatedly(Return(str));
EXPECT_CALL(m_mqtt,Get_campaign_id()).WillRepeatedly(Return("ebri:eb:honda.local:campaign-service:campaign:campaigns/"+ campaign_id));
auto response =  m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::ASSEMBLY_FINGERPRINT_MISMATCH,response.code);
}

TEST_F(Job_synchronization_test,persisted_state_handling_for_finishinng_state) {
std::string str =  "finishing";
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,get_job_state()).WillOnce(Return(cc::service::utils::Utils::INTERNAL_STATES::IDLE));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(true));
EXPECT_CALL(*m_persistency,read(testing::_)).WillRepeatedly(Return(str));
EXPECT_CALL(m_mqtt,Get_campaign_id()).WillRepeatedly(Return("ebri:eb:honda.local:campaign-service:campaign:campaigns/"+ campaign_id));
auto response =  m_job_control->pull(fingerprint);
    EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::ASSEMBLY_FINGERPRINT_MISMATCH,response.code);
}

TEST_F(Job_synchronization_test,FINGERPRINT_MISMATCH__) {
std::string str =  "downloaded";
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,get_job_state()).WillOnce(Return(cc::service::utils::Utils::INTERNAL_STATES::IDLE));
cadian::connectivity::Mqtt_interface::Download_request download_request;
    download_request.campaignId = "9449448784";
    download_request.frame_type_and_number = "Honda:id";
    download_request.ecu_list.push_back(ecu4);
    download_request.ecu_list.push_back(ecu5);
    download_request.ecu_list.push_back(ecu6);
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(true));
EXPECT_CALL(*m_persistency,read(testing::_)).WillRepeatedly(Return(str));
EXPECT_CALL(m_mqtt,get_Download_request_handler()).WillRepeatedly(Return(download_request));
EXPECT_CALL(m_mqtt,Get_campaign_id()).WillRepeatedly(Return(campaign_id));
auto response =  m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::ASSEMBLY_FINGERPRINT_MISMATCH,response.code);
}

TEST_F(Job_synchronization_test,FINGERPRINT_MISMATCH_____) {
std::string str =  "downloaded";
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,get_job_state()).WillOnce(Return(cc::service::utils::Utils::INTERNAL_STATES::IDLE));
cadian::connectivity::Mqtt_interface::Download_request download_request;
    download_request.campaignId = "9449448784";
    download_request.frame_type_and_number = "Honda:id";
    download_request.ecu_list.push_back(ecu7);
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(true));
EXPECT_CALL(*m_persistency,read(testing::_)).WillRepeatedly(Return(str));
EXPECT_CALL(m_mqtt,get_Download_request_handler()).WillRepeatedly(Return(download_request));
EXPECT_CALL(m_mqtt,Get_campaign_id()).WillRepeatedly(Return(campaign_id));
auto response =  m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::ASSEMBLY_FINGERPRINT_MISMATCH,response.code);
}

TEST_F(Job_synchronization_test,IF_PERSISTENCY_HAS_NO_HONDA_JOB_STATE) {
std::string str =  "prepared";
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,get_job_state()).WillOnce(Return(cc::service::utils::Utils::INTERNAL_STATES::IDLE));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
auto response =  m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::ASSEMBLY_FINGERPRINT_MISMATCH,response.code);
}

TEST_F(Job_synchronization_test,SET_IGNITION_STATUS) {
    cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
m_job_control->pull(fingerprint);
auto response =  m_job_control->pull(fingerprint);
}

TEST_F(Job_synchronization_test,MOSQUITTO_EXCEPTION) {
static const std::string what = "Connection Refused: Unacceptable protocol version";
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_job_state()).WillOnce(Return(cc::service::utils::Utils::INTERNAL_STATES::INSTALLING));
EXPECT_pull_throw_impl_(cadian::connectivity::Job_synchronization::Exception(what));
EXPECT_THROW(m_job_control->pull(fingerprint),cadian::connectivity::Job_synchronization::Exception);
}

TEST_F(Job_synchronization_test,MOSQUITTO_EXCEPTION_) {
static std::string what = "Run time error";
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).WillOnce(Return(cc::service::utils::Utils::INTERNAL_STATES::INSTALLING));
EXPECT_pull_throw_impl_(std::exception());
EXPECT_THROW(m_job_control->pull(fingerprint),cadian::connectivity::Job_synchronization::Exception);
}

TEST_F(Job_synchronization_test,campaign_stop_request_when_state_preparing) {
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).Times(3).WillOnce(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARED)).WillOnce(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARED)).WillOnce(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARING));
EXPECT_CALL(m_mqtt,is_download_request()).WillOnce(Return(true));
cadian::connectivity::Mqtt_interface::Download_request download_request;
download_request.campaignId = "9449448784";
download_request.frame_type_and_number = "Honda:id";
download_request.ecu_list.push_back(ecu4);
download_request.ecu_list.push_back(ecu5);
download_request.ecu_list.push_back(ecu6);
EXPECT_CALL(m_mqtt,get_Download_request_handler()).WillRepeatedly(Return(download_request));
EXPECT_CALL(m_mqtt,Get_campaign_id()).WillRepeatedly(Return("ebri:eb:honda.local:campaign-service:campaign:campaigns/"+ campaign_id));
m_job_control->pull(fingerprint);
m_job_control->pull(fingerprint);
EXPECT_CALL(m_mqtt,is_request_to_stop_campaign()).WillOnce(Return(true));
auto response = m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Job_state::pending,response.jobs[0].m_state);
}

TEST_F(Job_synchronization_test,campaign_stop_request_when_current_state_prepared) {
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).Times(3).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARED));
EXPECT_CALL(m_mqtt,is_request_to_stop_campaign()).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,reset_request_to_stop_campaign());
m_job_control->pull(fingerprint);
auto response = m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::SUCCESS,response.code);
}

TEST_F(Job_synchronization_test,campaign_stop_request_after_campaign_downloaded) {
      cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).Times(2).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::DOWNLOADED));
EXPECT_CALL(m_mqtt,is_request_to_stop_campaign()).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,reset_request_to_stop_campaign());
    m_job_control->pull(fingerprint);
      auto response = m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::SUCCESS,response.code);
}
 
TEST_F(Job_synchronization_test,campaign_stop_request_when_downloading) {
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).Times(3).WillOnce(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARED)).WillOnce(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARED)).WillOnce(Return(cc::service::utils::Utils::INTERNAL_STATES::DOWNLOAD_REQUESTED));
EXPECT_CALL(m_mqtt,is_download_request()).WillOnce(Return(true));
     cadian::connectivity::Mqtt_interface::Download_request download_request;
     download_request.campaignId = "9449448784";
     download_request.frame_type_and_number = "Honda:id";
     download_request.ecu_list.push_back(ecu4);
     download_request.ecu_list.push_back(ecu5);
     download_request.ecu_list.push_back(ecu6);
EXPECT_CALL(m_mqtt,get_Download_request_handler()).WillRepeatedly(Return(download_request));
EXPECT_CALL(m_mqtt,Get_campaign_id()).WillRepeatedly(Return("ebri:eb:honda.local:campaign-service:campaign:campaigns/"+ campaign_id));
    m_job_control->pull(fingerprint);
    m_job_control->pull(fingerprint);
EXPECT_CALL(m_mqtt,is_request_to_stop_campaign()).WillOnce(Return(true));
      auto response = m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Job_state::canceled,response.jobs[0].m_state);

}

TEST_F(Job_synchronization_test, pull_returns_vehicle_assembly_request_when_new_assemby_available) {
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
std::unordered_map<std::string, cadian::connectivity::Mqtt_interface::Vehicle_assembly> assembly_map {{"ADECU1", {"1.0.0", "210", "SN0000"}},{"TCU", {"1.0.0", "123", "SN0000"}},{"VCU", {"1.0.0", "123", "SN0000"}},{"PCU", {"1.0.0", "123", "SN0000"}}};
EXPECT_CALL(m_mqtt,get_job_state()).Times(3).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::FAILED));
EXPECT_CALL(m_mqtt,get_vehicle_assembly()).WillRepeatedly(Return(&assembly_map));
m_job_control->pull(fingerprint); 
EXPECT_CALL(m_mqtt,set_vehicle_assembly_request(false));
EXPECT_CALL(m_mqtt,is_vehicle_assembly_request()).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,is_new_assembly_available()).WillOnce(Return(true));
auto  response =  m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::SUCCESS,response.code);

}


TEST_F(Job_synchronization_test, pull_returns_vehicle_assembly_request__when_initial_assemby_available) {
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
std::unordered_map<std::string, cadian::connectivity::Mqtt_interface::Vehicle_assembly> assembly_map {{"ADECU1", {"1.0.0", "210", "SN0000"}},{"TCU", {"1.0.0", "123", "SN0000"}},{"VCU", {"1.0.0", "123", "SN0000"}},{"PCU", {"1.0.0", "123", "SN0000"}}};
EXPECT_CALL(m_mqtt,get_job_state()).Times(3).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARED));
EXPECT_CALL(m_mqtt,get_vehicle_assembly()).WillRepeatedly(Return(&assembly_map));
m_job_control->pull(fingerprint); 
EXPECT_CALL(m_mqtt,set_vehicle_assembly_request(false));
EXPECT_CALL(m_mqtt,is_vehicle_assembly_request()).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,is_initial_assembly_available()).WillOnce(Return(true));
auto  response =  m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::SUCCESS,response.code);
 }
 
TEST_F(Job_synchronization_test,Throw_vehicle_assembly_request_exception_when_assemby_not_available) {
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));  
EXPECT_CALL(m_mqtt,get_job_state()).Times(2).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARED));
EXPECT_CALL(m_mqtt,is_new_assembly_available()).WillOnce(Return(false));
EXPECT_CALL(m_mqtt,is_vehicle_assembly_request()).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,is_initial_assembly_available()).WillOnce(Return(false));
m_job_control->pull(fingerprint); 
auto  response =  m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::SUCCESS,response.code);
}
 
TEST_F(Job_synchronization_test,cancel_campaign_after_prepared) {
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARED));
EXPECT_CALL(m_mqtt,is_cancel_campaign_sent()).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,is_cancel_campaign_completed_sent()).WillOnce(Return(false));
m_job_control->pull(fingerprint); 
  auto  response =  m_job_control->pull(fingerprint);
 }

TEST_F(Job_synchronization_test,cancel_campaign_after_prepared_failed) {
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARE_FAILED));
EXPECT_CALL(m_mqtt,is_cancel_campaign_sent()).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,is_cancel_campaign_completed_sent()).WillOnce(Return(false));
m_job_control->pull(fingerprint); 
   auto  response =  m_job_control->pull(fingerprint);
 }
 
TEST_F(Job_synchronization_test,pull_returns_require_progress_request_when_downloading) {
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::DOWNLOADING));
EXPECT_CALL(status,get_campaign_download_progress()).WillOnce(Return(0));
EXPECT_CALL(m_mqtt,is_require_progress_request()).WillOnce(Return(true));
m_job_control->pull(fingerprint); 
 auto response =  m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::SUCCESS,response.code);
}


TEST_F(Job_synchronization_test,pull_returns_require_progress_request_when_activating) {
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::ACTIVATING));
EXPECT_CALL(status,get_campaign_install_activating_progress()).WillOnce(Return(0));
EXPECT_CALL(m_mqtt,is_require_progress_request()).WillOnce(Return(true));
m_job_control->pull(fingerprint); 
auto response =  m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::SUCCESS,response.code);
}


TEST_F(Job_synchronization_test,pull_returns_keyinfo_request) {
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARED));
EXPECT_CALL(m_mqtt,is_download_request()).Times(2).WillRepeatedly(Return(true));
cadian::connectivity::Mqtt_interface::Download_request download_request;
     download_request.campaignId = "9449448784";
     download_request.frame_type_and_number = "Honda:id";
     download_request.ecu_list.push_back(ecu4);
     download_request.ecu_list.push_back(ecu5);
     download_request.ecu_list.push_back(ecu6);
EXPECT_CALL(m_mqtt,get_Download_request_handler()).WillRepeatedly(Return(download_request));
EXPECT_CALL(m_mqtt,Get_campaign_id()).WillRepeatedly(Return("ebri:eb:honda.local:campaign-service:campaign:campaigns/"+ campaign_id));
   m_job_control->pull(fingerprint);
   m_job_control->pull(fingerprint);
   m_job_control->pull(fingerprint);
EXPECT_CALL(m_mqtt,is_keyinfo_request()).WillOnce(Return(true));
 auto response =  m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::SUCCESS,response.code);
EXPECT_EQ(vehicle_id,response.jobs[0].m_vehicle_id);
}


TEST_F(Job_synchronization_test,pull_returns_keyinfo_request_with_key_delay_flag) {
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARED));
EXPECT_CALL(m_mqtt,is_download_request()).Times(2).WillRepeatedly(Return(true));
cadian::connectivity::Mqtt_interface::Download_request download_request;
    download_request.campaignId = "9449448784";
    download_request.frame_type_and_number = "Honda:id";
    download_request.ecu_list.push_back(ecu4);
    download_request.ecu_list.push_back(ecu5);
    download_request.ecu_list.push_back(ecu6);
EXPECT_CALL(m_mqtt,get_Download_request_handler()).WillRepeatedly(Return(download_request));
EXPECT_CALL(m_mqtt,Get_campaign_id()).WillRepeatedly(Return("ebri:eb:honda.local:campaign-service:campaign:campaigns/"+ campaign_id));
    m_job_control->pull(fingerprint);
    m_job_control->pull(fingerprint);
    m_job_control->pull(fingerprint);
EXPECT_CALL(m_mqtt,is_keyinfo_request()).WillOnce(Return(true));
    m_job_control->pull(fingerprint);
EXPECT_CALL(m_mqtt,is_keyinfo_request()).WillOnce(Return(true));
 auto response =  m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::SUCCESS,response.code);
EXPECT_EQ(vehicle_id,response.jobs[0].m_vehicle_id);
}

TEST_F(Job_synchronization_test,pull_returns_signature_request) {
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARED));
EXPECT_CALL(m_mqtt,is_cancel_campaign_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
m_job_control->pull(fingerprint);
m_job_control->pull(fingerprint);
EXPECT_CALL(m_mqtt,is_signature_request()).WillOnce(Return(true));
auto response =  m_job_control->pull(fingerprint);
}


TEST_F(Job_synchronization_test,pull_returns_signature_request_with_key_delay) {
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARED));
EXPECT_CALL(m_mqtt,is_cancel_campaign_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,is_signature_request()).Times(3).WillRepeatedly(Return(true));
m_job_control->pull(fingerprint);
m_job_control->pull(fingerprint);
m_job_control->pull(fingerprint);
auto response =  m_job_control->pull(fingerprint);
 }
 


TEST_F(Job_synchronization_test,pull_returns_prepare_campaign_request_when_OTA_campaign_approved_and_finish_OTA_approved)
{

cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
    shared_memo.campaign_id = "9449448784";
    std::vector<std::future<void>> m_threadHandles;
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::IDLE));
EXPECT_CALL(m_mqtt,is_prepare_campaign_request()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,Get_campaign_id()).WillRepeatedly(Return( shared_memo.campaign_id));
EXPECT_CALL(*uds_client,
                GenericRequest(testing::_,testing::_,testing::_,testing::_)).WillOnce(Invoke([this,&m_threadHandles]
                                    (cadian::UDS::AddressInformation address_info, cadian::UDS::PDU request_VCU_OTA_approve,cadian::UDS::Callback negative_call,cadian::UDS::Callback  positive_call) {
                                          m_threadHandles.emplace_back(Async([positive_call, address_info,request_VCU_OTA_approve]() {
                    
                                        positive_call(address_info,request_VCU_OTA_approve);
                                        }
                                        ));
                                        }
                                        ));   
m_job_control->pull(fingerprint);
m_job_control->pull(fingerprint);
EXPECT_CALL(m_mqtt,is_inform_to_finish_request).WillOnce(Return(true)).WillOnce(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARED));
EXPECT_CALL(*uds_client,
                GenericRequest(testing::_,testing::_,testing::_,testing::_)).WillOnce(Invoke([this,&m_threadHandles]
                                    (cadian::UDS::AddressInformation address_info, cadian::UDS::PDU request_VCU_OTA_approve,cadian::UDS::Callback negative_call,cadian::UDS::Callback  positive_call) {
                                          m_threadHandles.emplace_back(Async([positive_call, address_info,request_VCU_OTA_approve]() {
                    
                                        positive_call(address_info,request_VCU_OTA_approve);
                                        }
                                        ));
                                        }
                                        ));  
  m_job_control->pull(fingerprint);
auto response =  m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::SUCCESS,response.code);
}


TEST_F(Job_synchronization_test,pull_returns_prepare_campaign_request_when_OTA_campaign_approved)
{
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
     shared_memo.campaign_id = "9449448784";
      std::vector<std::future<void>> m_threadHandles;
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::IDLE));
EXPECT_CALL(m_mqtt,is_prepare_campaign_request()).Times(2).WillOnce(Return(true)).WillOnce(Return(false));
EXPECT_CALL(m_mqtt,Get_campaign_id()).WillRepeatedly(Return( shared_memo.campaign_id));
EXPECT_CALL(*uds_client,
                GenericRequest(testing::_,testing::_,testing::_,testing::_)).WillOnce(Invoke([this,&m_threadHandles]
                                    (cadian::UDS::AddressInformation address_info, cadian::UDS::PDU request_VCU_OTA_approve,cadian::UDS::Callback negative_call,cadian::UDS::Callback  positive_call) {
                                          m_threadHandles.emplace_back(Async([positive_call, address_info,request_VCU_OTA_approve]() {
                    
                                        positive_call(address_info,request_VCU_OTA_approve);
                                        }
                                        ));
                                        }
                                        ));   
m_job_control->pull(fingerprint);
m_job_control->pull(fingerprint);
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARING));
   
auto response =  m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::SUCCESS,response.code);
}
TEST_F(Job_synchronization_test,If_pull_returns_prepare_campaign_request_with_OTA_campaign_not_approved)
{
 cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
     shared_memo.campaign_id = "9449448784";
     std::vector<std::future<void>> m_threadHandles;
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::IDLE));
EXPECT_CALL(m_mqtt,is_prepare_campaign_request()).Times(2).WillOnce(Return(true)).WillOnce(Return(false));
EXPECT_CALL(m_mqtt,Get_campaign_id()).WillRepeatedly(Return( shared_memo.campaign_id));
EXPECT_CALL(*uds_client,
                GenericRequest(testing::_,testing::_,testing::_,testing::_)).WillOnce(Invoke([this,&m_threadHandles]
                                    (cadian::UDS::AddressInformation address_info, cadian::UDS::PDU request_VCU_OTA_approve,cadian::UDS::Callback negative_call,cadian::UDS::Callback  positive_call) {
                                          m_threadHandles.emplace_back(Async([negative_call, address_info,request_VCU_OTA_approve]() {
                    
                                        negative_call(address_info,request_VCU_OTA_approve);
                                        }
                                        ));
                                        }
                                        ));   
m_job_control->pull(fingerprint);
m_job_control->pull(fingerprint);
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARING));
   
auto response =  m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::SUCCESS,response.code);
}

TEST_F(Job_synchronization_test,pull_returns_prepare_campaign_request)
{
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
    shared_memo.campaign_id = "9449448784";
m_job_control->pull(fingerprint);
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARING));
auto response =  m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::SUCCESS,response.code);
}


TEST_F(Job_synchronization_test,pull_returns_prepare_campaign_request_when_OTA_campaign_approved_and_finish_OTA_not_approved)
{
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
     shared_memo.campaign_id = "9449448784";
      std::vector<std::future<void>> m_threadHandles;
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::IDLE));
EXPECT_CALL(m_mqtt,is_prepare_campaign_request()).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,Get_campaign_id()).WillRepeatedly(Return( shared_memo.campaign_id));
EXPECT_CALL(*uds_client,
                GenericRequest(testing::_,testing::_,testing::_,testing::_)).WillOnce(Invoke([this,&m_threadHandles]
                                    (cadian::UDS::AddressInformation address_info, cadian::UDS::PDU request_VCU_OTA_approve,cadian::UDS::Callback negative_call,cadian::UDS::Callback  positive_call) {
                                          m_threadHandles.emplace_back(Async([positive_call, address_info,request_VCU_OTA_approve]() {
                    
                                        positive_call(address_info,request_VCU_OTA_approve);
                                        }
                                        ));
                                        }
                                        ));   
m_job_control->pull(fingerprint);
m_job_control->pull(fingerprint);
EXPECT_CALL(m_mqtt,is_inform_to_finish_request).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARED));
EXPECT_CALL(*uds_client,
                GenericRequest(testing::_,testing::_,testing::_,testing::_)).WillOnce(Invoke([this,&m_threadHandles]
                                    (cadian::UDS::AddressInformation address_info, cadian::UDS::PDU request_VCU_OTA_approve,cadian::UDS::Callback negative_call,cadian::UDS::Callback  positive_call) {
                                          m_threadHandles.emplace_back(Async([negative_call, address_info,request_VCU_OTA_approve]() {
                    
                                        negative_call(address_info,request_VCU_OTA_approve);
                                        }
                                        ));
                                        }
                                        ));  
auto response =  m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::SUCCESS,response.code);
}

TEST_F(Job_synchronization_test,inform_to_finish_request) {

cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::FAILED_ASSEMBLY_UPDATED));
EXPECT_CALL(m_mqtt,is_inform_to_finish_request()).WillOnce(Return(true));
     m_job_control->pull(fingerprint);
 auto response = m_job_control->pull(fingerprint);
 EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::SUCCESS,response.code);
}


TEST_F(Job_synchronization_test,pull_returns_prepare_campaign_request_when_OTA_campaign_not_approved)
{

cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
     shared_memo.campaign_id = "9449448784";
     std::vector<std::future<void>> m_threadHandles;
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::IDLE));
EXPECT_CALL(m_mqtt,is_prepare_campaign_request()).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,Get_campaign_id()).WillRepeatedly(Return( shared_memo.campaign_id));
EXPECT_CALL(*uds_client,
                GenericRequest(testing::_,testing::_,testing::_,testing::_)).WillOnce(Invoke([this,&m_threadHandles]
                                    (cadian::UDS::AddressInformation address_info, cadian::UDS::PDU request_VCU_OTA_approve,cadian::UDS::Callback negative_call,cadian::UDS::Callback  positive_call) {
                                          m_threadHandles.emplace_back(Async([negative_call, address_info,request_VCU_OTA_approve]() {
                    
                                        negative_call(address_info,request_VCU_OTA_approve);
                                        }
                                        ));
                                        }
                                        ));   
m_job_control->pull(fingerprint);
auto response =  m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::SUCCESS,response.code);
}

TEST_F(Job_synchronization_test,If_current_state_failed){
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).Times(2).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::FAILED));
EXPECT_CALL(m_mqtt,set_vehicle_assembly_request(true));
EXPECT_CALL(m_mqtt,is_mismatch_sent()).WillOnce(Return(false));
EXPECT_CALL(m_mqtt,mismatch_sent());
m_job_control->pull(fingerprint);
auto response =  m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::ASSEMBLY_FINGERPRINT_MISMATCH,response.code);
}


TEST_F(Job_synchronization_test,If_current_state_failed_when_cancel_campaign_is_not_sent) {
    cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
     shared_memo.campaign_id = "9449448784";
EXPECT_CALL(m_mqtt,get_job_state()).Times(3).WillOnce(Return(0)).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::STOPPED));
EXPECT_CALL(m_mqtt,is_vehicle_assembly_request()).WillOnce(Return(false));
EXPECT_CALL(m_mqtt,Get_campaign_id()).WillRepeatedly(Return( shared_memo.campaign_id));
EXPECT_CALL(m_mqtt,is_inform_to_finish_request()).WillOnce(Return(false));
EXPECT_CALL(m_mqtt,is_mismatch_sent()).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,is_cancel_campaign_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,set_cancel_campaign_sent(true));
 m_job_control->pull(fingerprint);
 m_job_control->pull(fingerprint);
}

TEST_F(Job_synchronization_test,If_current_state_failed_with_newly_assembly_available) {
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
     shared_memo.campaign_id = "9449448784";
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::FAILED));
EXPECT_CALL(m_mqtt,is_vehicle_assembly_request()).WillOnce(Return(false));
EXPECT_CALL(m_mqtt,is_inform_to_finish_request()).WillOnce(Return(false));
EXPECT_CALL(m_mqtt,is_cancel_campaign_sent()).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,Get_campaign_id()).WillRepeatedly(Return( shared_memo.campaign_id));
EXPECT_CALL(m_mqtt,is_new_assembly_available()).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,is_mismatch_sent()).WillOnce(Return(true));
 m_job_control->pull(fingerprint);
 m_job_control->pull(fingerprint);
} 

TEST_F(Job_synchronization_test,If_current_state_failed_when_assembly_is_not_ready) {
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::FAILED)); 
EXPECT_CALL(m_mqtt,is_vehicle_assembly_request()).WillOnce(Return(false));
EXPECT_CALL(m_mqtt,is_mismatch_sent()).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,is_inform_to_finish_request()).WillOnce(Return(false));
EXPECT_CALL(m_mqtt,is_cancel_campaign_sent()).WillOnce(Return(true));
EXPECT_CALL(m_mqtt,is_new_assembly_available()).WillOnce(Return(false));
 m_job_control->pull(fingerprint);
 m_job_control->pull(fingerprint);
}

TEST_F(Job_synchronization_test,pull_returns_download_request) {
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).Times(2).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARED));
EXPECT_CALL(m_mqtt,is_download_request()).WillOnce(Return(true));
  cadian::connectivity::Mqtt_interface::Download_request download_request;
     download_request.campaignId = "9449448784";
     download_request.frame_type_and_number = "Honda:id";
     download_request.ecu_list.push_back(ecu4);
     download_request.ecu_list.push_back(ecu5);
     download_request.ecu_list.push_back(ecu6);
EXPECT_CALL(m_mqtt,get_Download_request_handler()).WillRepeatedly(Return(download_request));
    m_job_control->pull(fingerprint);
 auto response = m_job_control->pull(fingerprint);
EXPECT_EQ(vehicle_id,response.jobs[0].m_vehicle_id);
EXPECT_EQ("ebri:eb:honda.local:campaign-service:campaign:campaigns/"+ campaign_id,response.jobs[0].m_campaign);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::SUCCESS,response.code);
}

TEST_F(Job_synchronization_test,push_status_when_job_state_failed) {
      cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
    EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
  EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
     EXPECT_CALL(m_mqtt,get_job_state()).Times(2).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARED));
    EXPECT_CALL(m_mqtt,is_download_request()).WillOnce(Return(true));
     cadian::connectivity::Mqtt_interface::Download_request download_request;
     download_request.campaignId = "9449448784";
     download_request.frame_type_and_number = "Honda:id";
     download_request.ecu_list.push_back(ecu4);
     download_request.ecu_list.push_back(ecu5);
        download_request.ecu_list.push_back(ecu6);
    EXPECT_CALL(m_mqtt,get_Download_request_handler()).WillRepeatedly(Return(download_request));

    m_job_control->pull(fingerprint);
 auto response = m_job_control->pull(fingerprint);
 response.jobs[0].m_state = cadian::connectivity::Job_synchronization::Job_state::failed;
 EXPECT_CALL(m_mqtt,get_job_state()).Times(2).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::INSTALLING));
 m_job_control->push(fingerprint,response.jobs);
}

TEST_F(Job_synchronization_test,push_status_when_job_state_activating) {
     cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).Times(2).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARED));
EXPECT_CALL(m_mqtt,is_download_request()).WillOnce(Return(true));
     cadian::connectivity::Mqtt_interface::Download_request download_request;
     download_request.campaignId = "9449448784";
     download_request.frame_type_and_number = "Honda:id";
     download_request.ecu_list.push_back(ecu4);
     download_request.ecu_list.push_back(ecu5);
     download_request.ecu_list.push_back(ecu6);
EXPECT_CALL(m_mqtt,get_Download_request_handler()).WillRepeatedly(Return(download_request));
    m_job_control->pull(fingerprint);
 auto response = m_job_control->pull(fingerprint);
 response.jobs[0].m_state = cadian::connectivity::Job_synchronization::Job_state::failed;
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::ACTIVATING));
 m_job_control->push(fingerprint,response.jobs);
}

TEST_F(Job_synchronization_test,push_status_when_job_state_doing_rollback) {
     cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).Times(2).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARED));
EXPECT_CALL(m_mqtt,is_download_request()).WillOnce(Return(true));
     cadian::connectivity::Mqtt_interface::Download_request download_request;
     download_request.campaignId = "9449448784";
     download_request.frame_type_and_number = "Honda:id";
     download_request.ecu_list.push_back(ecu4);
     download_request.ecu_list.push_back(ecu5);
     download_request.ecu_list.push_back(ecu6);
EXPECT_CALL(m_mqtt,get_Download_request_handler()).WillRepeatedly(Return(download_request));

    
 m_job_control->pull(fingerprint);
 auto response = m_job_control->pull(fingerprint);
 response.jobs[0].m_state = cadian::connectivity::Job_synchronization::Job_state::failed;
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::DOING_ROLLBACK));
 m_job_control->push(fingerprint,response.jobs);

}

TEST_F(Job_synchronization_test,push_failed_when_job_state_aborted) {
     cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).Times(3).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARED));
EXPECT_CALL(m_mqtt,is_download_request()).WillOnce(Return(true));
     cadian::connectivity::Mqtt_interface::Download_request download_request;
     download_request.campaignId = "9449448784";
     download_request.frame_type_and_number = "Honda:id";
     download_request.ecu_list.push_back(ecu4);
     download_request.ecu_list.push_back(ecu5);
     download_request.ecu_list.push_back(ecu6);
EXPECT_CALL(m_mqtt,get_Download_request_handler()).WillRepeatedly(Return(download_request));

    
    m_job_control->pull(fingerprint);
 auto response = m_job_control->pull(fingerprint);
 response.jobs[0].m_state = cadian::connectivity::Job_synchronization::Job_state::aborted;
 m_job_control->push(fingerprint,response.jobs);

}

TEST_F(Job_synchronization_test,when_stop_was_received_due_to_prepare_failed) {
     cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
    EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
  EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
   EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
    EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
    EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(false));
    EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
    EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARE_FAILED));
    EXPECT_CALL(m_mqtt,is_request_to_stop_in_preparing()).WillOnce(Return(true));
  m_job_control->pull(fingerprint);
 auto response = m_job_control->pull(fingerprint);
}

TEST_F(Job_synchronization_test,when_stop_was_received_during_preparing_state) {
     cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARED));
EXPECT_CALL(m_mqtt,is_request_to_stop_in_preparing()).WillOnce(Return(true));
  m_job_control->pull(fingerprint);
 auto response = m_job_control->pull(fingerprint);
}


TEST_F(Job_synchronization_test,when_current_state_is_prepare_failed) {
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARE_FAILED));
EXPECT_CALL(m_mqtt,is_cancel_campaign_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,is_cancel_campaign_completed_sent()).WillRepeatedly(Return(false));
m_job_control->pull(fingerprint);
 auto response = m_job_control->pull(fingerprint);
}

TEST_F(Job_synchronization_test,when_current_state_is_PREPARING_0X22) {
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(false));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false)); 
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARING_0X22));
   m_job_control->pull(fingerprint);
 auto response = m_job_control->pull(fingerprint);
}

TEST_F(Job_synchronization_test,download_requested_when_state_is_not_prepared) {
    cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARED));
EXPECT_CALL(m_mqtt,is_download_request()).WillOnce(Return(true));
m_job_control->pull(fingerprint);
 auto response = m_job_control->pull(fingerprint);
EXPECT_EQ(cadian::connectivity::Job_synchronization::Response::Code::SUCCESS,response.code);
}

TEST_F(Job_synchronization_test,exception_when_campaign_id_lost) {
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARED));
EXPECT_CALL(m_mqtt,is_download_request()).WillOnce(Return(true));
    cadian::connectivity::Mqtt_interface::Download_request download_request;
    download_request.campaignId = "9449448784";
    download_request.frame_type_and_number = "Honda:id";
    download_request.ecu_list.push_back(ecu4);
    download_request.ecu_list.push_back(ecu5);
    download_request.ecu_list.push_back(ecu6);
EXPECT_CALL(m_mqtt,get_Download_request_handler()).Times(6).WillOnce(Return(download_request)).WillOnce(Return(download_request)).WillOnce(Return(download_request)).WillOnce(Return(download_request));
EXPECT_CALL(m_mqtt,Get_campaign_id()).WillRepeatedly(Return("ebri:eb:honda.local:campaign-service:campaign:campaigns/"+ campaign_id));
    m_job_control->pull(fingerprint);
    m_job_control->pull(fingerprint);
}

TEST_F(Job_synchronization_test,exception_for_no_download_url) {
cadian::connectivity::Mqtt_interface::Shared_memory shared_memo(*m_persistency_stub);
EXPECT_CALL(m_mqtt,is_connected()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,is_iginition_status_sent()).WillRepeatedly(Return(true));
EXPECT_CALL(m_mqtt,get_persistency_service()).WillRepeatedly(Return(m_persistency));
EXPECT_CALL(*m_persistency,has(testing::_)).WillRepeatedly(Return(false));
EXPECT_CALL(m_mqtt,get_job_state()).WillRepeatedly(Return(cc::service::utils::Utils::INTERNAL_STATES::PREPARED));
EXPECT_CALL(m_mqtt,is_download_request()).WillOnce(Return(true));
    cadian::connectivity::Mqtt_interface::Download_request download_request;
    download_request.campaignId = "9449448784";
    download_request.frame_type_and_number = "Honda:id";
    download_request.ecu_list.push_back(ecu7);
EXPECT_CALL(m_mqtt,get_Download_request_handler()).WillRepeatedly(Return(download_request));
EXPECT_CALL(m_mqtt,Get_campaign_id()).WillRepeatedly(Return("ebri:eb:honda.local:campaign-service:campaign:campaigns/"+ campaign_id));
m_job_control->pull(fingerprint);
m_job_control->pull(fingerprint);
}
}
