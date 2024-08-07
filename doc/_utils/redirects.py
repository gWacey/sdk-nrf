"""
Copyright (c) 2022 Nordic Semiconductor
SPDX-License-Identifier: Apache-2.0

This module contains per-docset variables with a list of tuples
(old_url, new_url) for pages that need a redirect. This list allows redirecting
old URLs (caused by, e.g., reorganizing doc directories)

Notes:
    - Please keep lists sorted alphabetically.
    - URLs must be relative to document root (with NO leading slash), and
      without the html extension).

Examples:

    ("old/README", "new/index")
"""

NRF = [
    ("introduction", "index"),
    ("app_boards", "config_and_build/board_support"),
    ("app_dev/board_support/index", "config_and_build/board_support"),
    ("config_and_build/board_support", "config_and_build/board_support/index"),
    ("config_and_build/board_support/index", "app_dev/board_support/index"),
    ("config_and_build/board_support/board_names", "app_dev/board_support/board_names"),
    ("config_and_build/board_support/defining_custom_board", "app_dev/board_support/defining_custom_board"),
    ("config_and_build/board_support/processing_environments", "app_dev/board_support/processing_environments"),
    ("config_and_build/companion_components", "app_dev/companion_components"),
    ("config_and_build/output_build_files", "config_and_build/configuring_app/output_build_files"),
    ("config_and_build/configuring_app/output_build_files", "app_dev/config_and_build/output_build_files"),
    ("config_and_build/pin_control", "config_and_build/configure_app/hardware/pin_control"),
    ("config_and_build/use_gpio_pin_directly", "config_and_build/configure_app/hardware/use_gpio_pin_directly"),
    ("ug_bootloader", "config_and_build/bootloaders_and_dfu/bootloader"),
    ("app_dev/bootloaders_and_dfu/bootloader", "config_and_build/bootloaders_and_dfu/bootloader"),
    ("app_dev/bootloaders_and_dfu/bootloader", "config_and_build/bootloaders/bootloader"),
    ("config_and_build/bootloaders/bootloader", "config_and_build/bootloaders_dfu/mcuboot_nsib/bootloader"),
    ("config_and_build/bootloaders_dfu/mcuboot_nsib/bootloader", "app_dev/bootloaders_dfu/mcuboot_nsib/bootloader"),
    ("ug_bootloader_adding", "config_and_build/bootloaders_and_dfu/bootloader_adding"),
    ("app_dev/bootloaders_and_dfu/bootloader_adding", "config_and_build/bootloaders_and_dfu/bootloader_adding"),
    ("app_dev/bootloaders_and_dfu/bootloader_adding", "config_and_build/bootloaders/bootloader_adding"),
    ("config_and_build/bootloaders/bootloader_adding", "config_and_build/bootloaders_dfu/mcuboot_nsib/bootloader_adding"),
    ("config_and_build/bootloaders_dfu/mcuboot_nsib/bootloader_adding", "app_dev/bootloaders_dfu/mcuboot_nsib/bootloader_adding"),
    ("config_and_build/bootloaders/bootloader_adding_sysbuild", "config_and_build/bootloaders_dfu/mcuboot_nsib/bootloader_adding_sysbuild"),
    ("config_and_build/bootloaders_dfu/mcuboot_nsib/bootloader_adding_sysbuild", "app_dev/bootloaders_dfu/mcuboot_nsib/bootloader_adding_sysbuild"),
    ("ug_bootloader_config", "config_and_build/bootloaders_and_dfu/bootloader_config"),
    ("app_dev/bootloaders_and_dfu/bootloader_config", "config_and_build/bootloaders_and_dfu/bootloader_config"),
    ("app_dev/bootloaders_and_dfu/bootloader_config", "config_and_build/bootloaders/bootloader_config"),
    ("config_and_build/bootloaders/bootloader_config", "config_and_build/bootloaders_dfu/mcuboot_nsib/bootloader_config"),
    ("config_and_build/bootloaders_dfu/mcuboot_nsib/bootloader_config", "app_dev/bootloaders_dfu/mcuboot_nsib/bootloader_config"),
    ("config_and_build/bootloaders/bootloader_downgrade_protection", "config_and_build/bootloaders_dfu/mcuboot_nsib/bootloader_downgrade_protection"),
    ("config_and_build/bootloaders_dfu/mcuboot_nsib/bootloader_downgrade_protection", "app_dev/bootloaders_dfu/mcuboot_nsib/bootloader_downgrade_protection"),
    ("ug_bootloader_external_flash", "config_and_build/bootloaders_and_dfu/bootloader_external_flash"),
    ("app_dev/bootloaders_and_dfu/bootloader_external_flash", "config_and_build/bootloaders_and_dfu/bootloader_external_flash"),
    ("app_dev/bootloaders_and_dfu/bootloader_external_flash", "config_and_build/bootloaders/bootloader_external_flash"),
    ("config_and_build/bootloaders/bootloader_external_flash", "config_and_build/bootloaders_dfu/mcuboot_nsib/bootloader_partitioning"),
    ("config_and_build/bootloaders_dfu/mcuboot_nsib/bootloader_partitioning", "app_dev/bootloaders_dfu/mcuboot_nsib/bootloader_partitioning"),
    ("ug_bootloader_testing", "config_and_build/bootloaders_and_dfu/bootloader_testing"),
    ("app_dev/bootloaders_and_dfu/bootloader_testing", "config_and_build/bootloaders_and_dfu/bootloader_testing"),
    ("app_dev/bootloaders_and_dfu/bootloader_testing", "config_and_build/bootloaders/bootloader_testing"),
    ("config_and_build/bootloaders/bootloader_testing", "config_and_build/bootloaders_dfu/mcuboot_nsib/bootloader_mcuboot_nsib"),
    ("config_and_build/bootloaders_dfu/mcuboot_nsib/bootloader_mcuboot_nsib", "app_dev/bootloaders_dfu/mcuboot_nsib/bootloader_mcuboot_nsib"),
    ("config_and_build/bootloaders/bootloader_quick_start", "config_and_build/bootloaders_dfu/mcuboot_nsib/bootloader_quick_start"),
    ("config_and_build/bootloaders_dfu/mcuboot_nsib/bootloader_quick_start", "app_dev/bootloaders_dfu/mcuboot_nsib/bootloader_quick_start"),
    ("config_and_build/bootloaders/bootloader_signature_keys", "config_and_build/bootloaders_dfu/mcuboot_nsib/bootloader_signature_keys"),
    ("config_and_build/bootloaders_dfu/mcuboot_nsib/bootloader_signature_keys", "app_dev/bootloaders_dfu/mcuboot_nsib/bootloader_signature_keys"),
    ("ug_fw_update", "config_and_build/bootloaders_and_dfu/fw_update"),
    ("app_dev/bootloaders_and_dfu/fw_update", "config_and_build/bootloaders_and_dfu/fw_update"),
    ("config_and_build/bootloaders_and_dfu/fw_update", "config_and_build/dfu/index"),
    ("app_bootloaders", "config_and_build/bootloaders_and_dfu/index"),
    ("app_dev/bootloaders_and_dfu/index", "config_and_build/bootloaders_and_dfu/index"),
    ("config_and_build/bootloaders_and_dfu/index", "config_and_build/bootloaders_dfu/index"),
    ("config_and_build/bootloaders_dfu/index", "app_dev/bootloaders_dfu/index"),
    ("config_and_build/dfu/index", "app_dev/dfu/index"),
    ("app_dev/dfu/index", "app_dev/bootloaders_dfu/index"),
    ("config_and_build/dfu/dfu_image_versions", "config_and_build/bootloaders/bootloader_dfu_image_versions"),
    ("config_and_build/bootloaders/bootloader_dfu_image_versions", "config_and_build/bootloaders_dfu/mcuboot_nsib/bootloader_dfu_image_versions"),
    ("config_and_build/bootloaders_dfu/mcuboot_nsib/bootloader_dfu_image_versions", "app_dev/bootloaders_dfu/mcuboot_nsib/bootloader_dfu_image_versions"),
    ("ug_logging", "test_and_optimize/logging"),
    ("app_dev/logging/index", "test_and_optimize/logging"),
    ("ug_multi_image", "config_and_build/multi_image"),
    ("app_dev/multi_image/index", "config_and_build/multi_image"),
    ("config_and_build/multi_image", "app_dev/config_and_build/multi_image"),
    ("app_opt", "test_and_optimize/optimizing/index"),
    ("app_dev/optimizing/index", "test_and_optimize/optimizing/index"),
    ("app_memory", "test_and_optimize/optimizing/memory"),
    ("app_dev/optimizing/memory", "test_and_optimize/optimizing/memory"),
    ("app_power_opt", "test_and_optimize/optimizing/power"),
    ("app_dev/optimizing/power", "test_and_optimize/optimizing/power"),
    ("device_guides", "app_dev"),
    ("ug_pinctrl", "device_guides/pin_control"),
    ("app_dev/pin_control/index", "device_guides/pin_control"),
    ("device_guides/pin_control", "config_and_build/configuring_app/hardware/pin_control"),
    ("config_and_build/configuring_app/hardware/pin_control", "app_dev/config_and_build/hardware/pin_control"),
    ("ug_unity_testing", "test_and_optimize/testing_unity_cmock"),
    ("app_dev/testing_unity_cmock/index", "test_and_optimize/testing_unity_cmock"),
    ("test_and_optimize/testing_unity_cmock", "test_and_optimize/test_framework/testing_unity_cmock"),
    ("ug_tfm", "security/tfm"),
    ("app_dev/tfm/index", "security/tfm"),
    ("app_dev/ap_protect/index", "security/ap_protect"),
    ("app_build_system", "config_and_build/config_and_build_system"),
    ("app_dev/build_and_config_system/index", "config_and_build/config_and_build_system"),
    ("config_and_build/config_and_build_system", "app_dev/config_and_build/config_and_build_system"),
    ("config_and_build/configuring_app/sysbuild/index", "app_dev/config_and_build/sysbuild/index"),
    ("config_and_build/configuring_app/sysbuild/sysbuild_images", "app_dev/config_and_build/sysbuild/sysbuild_images"),
    ("config_and_build/configuring_app/sysbuild/sysbuild_forced_options", "app_dev/config_and_build/sysbuild/sysbuild_forced_options"),
    ("config_and_build/configuring_app/sysbuild/zephyr_samples_sysbuild", "app_dev/config_and_build/sysbuild/zephyr_samples_sysbuild"),
    ("config_and_build/configuring_app/sysbuild/sysbuild_configuring_west", "app_dev/config_and_build/sysbuild/sysbuild_configuring_west"),
    ("config_and_build/configuring_app/advanced_building", "app_dev/config_and_build/building"),
    ("config_and_build/configuring_app/building", "app_dev/config_and_build/building"),
    ("config_and_build/configuring_app/cmake/index", "app_dev/config_and_build/cmake/index"),
    ("config_and_build/configuring_app/hardware/index", "app_dev/config_and_build/hardware/index"),
    ("config_and_build/configuring_app/hardware/use_gpio_pin_directly", "app_dev/config_and_build/hardware/use_gpio_pin_directly"),
    ("config_and_build/configuring_app/kconfig/index", "app_dev/config_and_build/kconfig/index"),
    ("ug_radio_coex", "device_guides/wifi_coex"),
    ("app_dev/wifi_coex/index", "device_guides/wifi_coex"),
    ("device_guides/wifi_coex", "app_dev/device_guides/wifi_coex"),
    ("ug_radio_fem", "device_guides/working_with_fem"),
    ("app_dev/working_with_fem/index", "device_guides/working_with_fem"),
    ("device_guides/working_with_fem","device_guides/fem/index"),
    ("device_guides/fem/index","app_dev/device_guides/fem/index"),
    ("device_guides/fem/21540ek_dev_guide","app_dev/device_guides/fem/21540ek_dev_guide"),
    ("device_guides/fem/fem_incomplete_connections","app_dev/device_guides/fem/fem_incomplete_connections"),
    ("device_guides/fem/fem_mpsl_fem_only","app_dev/device_guides/fem/fem_mpsl_fem_only"),
    ("device_guides/fem/fem_nrf21540_gpio","app_dev/device_guides/fem/fem_nrf21540_gpio"),
    ("device_guides/fem/fem_nrf21540_gpio_spi","app_dev/device_guides/fem/fem_nrf21540_gpio_spi"),
    ("device_guides/fem/fem_nRF21540_optional_properties","app_dev/device_guides/fem/fem_nRF21540_optional_properties"),
    ("device_guides/fem/fem_power_models","app_dev/device_guides/fem/fem_power_models"),
    ("device_guides/fem/fem_simple_gpio","app_dev/device_guides/fem/fem_simple_gpio"),
    ("device_guides/fem/fem_software_support","app_dev/device_guides/fem/fem_software_support"),
    ("ug_dev_model", "releases_and_maturity/dev_model"),
    ("dev_model", "releases_and_maturity/dev_model"),
    ("dm_adding_code", "releases_and_maturity/developing/adding_code"),
    ("developing/adding_code", "releases_and_maturity/developing/adding_code"),
    ("releases_and_maturity/developing/adding_code", "dev_model_and_contributions/adding_code"),
    ("dm_code_base", "releases_and_maturity/developing/code_base"),
    ("developing/code_base", "releases_and_maturity/developing/code_base"),
    ("releases_and_maturity/developing/code_base", "dev_model_and_contributions/code_base"),
    ("dm_managing_code", "developing/managing_code"),
    ("developing/managing_code", "releases_and_maturity/developing/managing_code"),
    ("releases_and_maturity/developing/managing_code", "dev_model_and_contributions/managing_code"),
    ("dm_ncs_distro", "releases_and_maturity/developing/ncs_distro"),
    ("developing/ncs_distro", "releases_and_maturity/developing/ncs_distro"),
    ("releases_and_maturity/developing/ncs_distro", "dev_model_and_contributions/ncs_distro"),
    ("release_notes", "releases_and_maturity/release_notes"),
    ("migration/migration_guide_1.x_to_2.x", "releases_and_maturity/migration/migration_guide_1.x_to_2.x"),
    ("software_maturity", "releases_and_maturity/software_maturity"),
    ("known_issues","releases_and_maturity/known_issues"),
    ("doc_build", "documentation/build"),
    ("doc_build_process", "documentation/build_process"),
    ("doc_structure", "documentation/structure"),
    ("doc_styleguide", "documentation/styleguide"),
    ("doc_templates", "documentation/templates"),
    ("documentation/build", "dev_model_and_contributions/documentation/build"),
    ("documentation/build_process", "dev_model_and_contributions/documentation/doc_build_process"),
    ("documentation/structure", "dev_model_and_contributions/documentation/structure"),
    ("documentation/styleguide", "dev_model_and_contributions/documentation/styleguide"),
    ("documentation/templates", "dev_model_and_contributions/documentation/templates"),
    ("ecosystems_integrations", "integrations"),
    ("ug_bt_fast_pair", "external_comp/bt_fast_pair"),
    ("ug_edge_impulse", "external_comp/edge_impulse"),
    ("ug_memfault", "external_comp/memfault"),
    ("ug_nrf_cloud", "external_comp/nrf_cloud"),
    ("gs_assistant", "installation/assistant"),
    ("getting_started", "installation"),
    ("getting_started/assistant", "installation/install_ncs"),
    ("installation/assistant", "installation/install_ncs"),
    ("gs_installing", "installation/installing"),
    ("getting_started/installing", "installation/install_ncs"),
    ("installation/installing", "installation/install_ncs"),
    ("create_application", "app_dev/create_application"),
    ("gs_modifying", "config_and_build/modifying"),
    ("getting_started/modifying", "config_and_build/modifying"),
    ("config_and_build/modifying", "config_and_build/configuring_app/index"),
    ("config_and_build/configuring_app/index", "config_and_build/index"),
    ("gs_programming", "config_and_build/programming"),
    ("getting_started/programming", "config_and_build/programming"),
    ("config_and_build/programming", "app_dev/programming"),
    ("config_and_build", "config_and_build/index"),
    ("config_and_build/index", "app_dev/config_and_build/index"),
    ("gs_recommended_versions", "installation/recommended_versions"),
    ("getting_started/recommended_versions", "installation/recommended_versions"),
    ("gs_testing", "test_and_optimize/testing"),
    ("getting_started/testing", "test_and_optimize/testing"),
    ("test_and_optimize/testing", "test_and_optimize"),
    ("gs_updating", "installation/updating"),
    ("getting_started/updating", "installation/updating"),
    ("ug_nrf70", "device_guides/nrf70"),
    ("nrf70", "device_guides/nrf70"),
    ("device_guides/nrf70/index", "app_dev/device_guides/nrf70/index"),
    ("ug_nrf91", "device_guides/nrf91"),
    ("nrf91", "device_guides/nrf91"),
    ("device_guides/nrf91", "device_guides/nrf91/index"),
    ("device_guides/nrf91/index", "app_dev/device_guides/nrf91/index"),
    ("ug_ble_controller", "protocols/ble/index"),
    ("ug_bt_mesh_architecture", "protocols/bt_mesh/architecture"),
    ("ug_bt_mesh_concepts", "protocols/bt_mesh/concepts"),
    ("ug_bt_mesh_configuring", "protocols/bt_mesh/configuring"),
    ("ug_bt_mesh_fota", "protocols/bt_mesh/fota"),
    ("ug_bt_mesh", "protocols/bt_mesh/index"),
    ("ug_bt_mesh_model_config_app", "protocols/bt_mesh/model_config_app"),
    ("ug_bt_mesh_node_removal", "protocols/bt_mesh/node_removal"),
    ("ug_bt_mesh_reserved_ids", "protocols/bt_mesh/reserved_ids"),
    ("ug_bt_mesh_supported_features", "protocols/bt_mesh/supported_features"),
    ("ug_bt_mesh_vendor_model", "protocols/bt_mesh/vendor_model/index"),
    ("ug_bt_mesh_vendor_model_chat_sample_walk_through", "protocols/bt_mesh/vendor_model/chat_sample_walk_through"),
    ("ug_bt_mesh_vendor_model_dev_overview", "protocols/bt_mesh/vendor_model/dev_overview"),
    ("ug_esb","protocols/esb/index"),
    ("ug_gzll","protocols/gazell/gzll"),
    ("ug_gzp","protocols/gazell/gzp"),
    ("ug_gz","protocols/gazell/index"),
    ("ug_matter_device_attestation","protocols/matter/end_product/attestation"),
    ("ug_matter_device_bootloader","protocols/matter/end_product/bootloader"),
    ("ug_matter_device_certification","protocols/matter/end_product/certification"),
    ("ug_matter_ecosystems_certification","protocols/matter/end_product/ecosystems_certification"),
    ("ug_matter_device_dcl","protocols/matter/end_product/dcl"),
    ("ug_matter_device_factory_provisioning","protocols/matter/end_product/factory_provisioning"),
    ("ug_matter_intro_device","protocols/matter/end_product/index"),
    ("ug_matter_device_prerequisites","protocols/matter/end_product/prerequisites"),
    ("ug_matter_gs_adding_clusters","protocols/matter/getting_started/adding_clusters"),
    ("ug_matter_gs_advanced_kconfigs","protocols/matter/getting_started/advanced_kconfigs"),
    ("ug_matter_hw_requirements","protocols/matter/getting_started/hw_requirements"),
    ("ug_matter_intro_gs","protocols/matter/getting_started/index"),
    ("ug_matter_gs_kconfig","protocols/matter/getting_started/kconfig"),
    ("ug_matter_gs_testing","protocols/matter/getting_started/testing/index"),
    ("ug_matter_gs_testing_thread_one_otbr","protocols/matter/getting_started/testing/thread_one_otbr"),
    ("ug_matter_gs_testing_thread_separate_otbr_android","protocols/matter/getting_started/testing/thread_separate_otbr_android"),
    ("ug_matter_gs_testing_thread_separate_linux_macos","protocols/matter/getting_started/testing/thread_separate_otbr_linux_macos"),
    ("ug_matter_gs_testing_wifi_mobile","protocols/matter/getting_started/testing/wifi_mobile"),
    ("ug_matter_gs_testing_wifi_pc","protocols/matter/getting_started/testing/wifi_pc"),
    ("ug_matter_gs_tools","protocols/matter/getting_started/tools"),
    ("ug_matter","protocols/matter/index"),
    ("ug_matter_overview_architecture","protocols/matter/overview/architecture"),
    ("ug_matter_overview_commissioning","protocols/matter/overview/commisioning"),
    ("ug_matter_overview_data_model","protocols/matter/overview/data_model"),
    ("ug_matter_overview_dev_model","protocols/matter/overview/dev_model"),
    ("ug_matter_overview_dfu","protocols/matter/overview/dfu"),
    ("ug_matter_intro_overview","protocols/matter/overview/index"),
    ("ug_matter_overview_int_model","protocols/matter/overview/int_model"),
    ("ug_matter_overview_architecture_integration","protocols/matter/overview/integration"),
    ("ug_matter_overview_multi_fabrics","protocols/matter/overview/multi_fabrics"),
    ("ug_matter_overview_network_topologies","protocols/matter/overview/network_topologies"),
    ("ug_matter_overview_security","protocols/matter/overview/security"),
    ("ug_multiprotocol_support","protocols/multiprotocol/index"),
    ("ug_nfc","protocols/nfc/index"),
    ("ug_thread_certification","protocols/thread/certification"),
    ("ug_thread_configuring","protocols/thread/configuring"),
    ("ug_thread","protocols/thread/index"),
    ("ug_thread_architectures","protocols/thread/overview/architectures"),
    ("ug_thread_commissioning","protocols/thread/overview/commissioning"),
    ("ug_thread_communication","protocols/thread/overview/communication"),
    ("ug_thread_overview","protocols/thread/overview/index"),
    ("ug_thread_ot_integration","protocols/thread/overview/ot_integration"),
    ("ug_thread_ot_memory","protocols/thread/overview/ot_memory"),
    ("ug_thread_supported_features","protocols/thread/overview/supported_features"),
    ("ug_thread_prebuilt_libs","protocols/thread/prebuilt_libs"),
    ("ug_thread_tools","protocols/thread/tools"),
    ("ug_wifi","protocols/wifi/index"),
    ("ug_zigbee_adding_clusters","protocols/zigbee/adding_clusters"),
    ("ug_zigbee_architectures","protocols/zigbee/architectures"),
    ("ug_zigbee_commissioning","protocols/zigbee/commissioning"),
    ("ug_zigbee_configuring","protocols/zigbee/configuring"),
    ("ug_zigbee_configuring_libraries","protocols/zigbee/configuring_libraries"),
    ("ug_zigbee_configuring_zboss_traces","protocols/zigbee/configuring_zboss_traces"),
    ("ug_zigbee","protocols/zigbee/index"),
    ("ug_zigbee_memory","protocols/zigbee/memory"),
    ("ug_zigbee_other_ecosystems","protocols/zigbee/other_ecosystems"),
    ("ug_zigbee_qsg","protocols/zigbee/qsg"),
    ("ug_zigbee_supported_features","protocols/zigbee/supported_features"),
    ("ug_zigbee_tools","protocols/zigbee/tools"),
    ("samples/samples_bl","samples/bl"),
    ("samples/samples_crypto","samples/crypto"),
    ("samples/samples_edge","samples/edge"),
    ("samples/samples_gazell","samples/gazell"),
    ("samples/samples_matter","samples/matter"),
    ("samples/samples_nfc","samples/nfc"),
    ("samples/samples_nrf5340","samples/nrf5340"),
    ("samples/samples_nrf9160","samples/cellular"),
    ("samples/samples_other","samples/other"),
    ("samples/samples_tfm","samples/tfm"),
    ("samples/samples_thread","samples/thread"),
    ("samples/samples_wifi","samples/wifi"),
    ("samples/samples_zigbee","samples/zigbee"),
    ("samples/wifi/sr_coex/README","samples/wifi/ble_coex/README"),
    ("security_chapter","security/security"),
    ("security","security/security"),
    ("ug_nrf52", "device_guides/nrf52"),
    ("nrf52", "device_guides/nrf52"),
    ("ug_nrf52_developing","device_guides/working_with_nrf/nrf52/developing"),
    ("working_with_nrf/nrf52/developing","device_guides/working_with_nrf/nrf52/developing"),
    ("device_guides/working_with_nrf/nrf52/developing","device_guides/nrf52/index"),
    ("device_guides/nrf52/index","app_dev/device_guides/nrf52/index"),
    ("ug_nrf52_features","device_guides/working_with_nrf/nrf52/features"),
    ("working_with_nrf/nrf52/features","device_guides/working_with_nrf/nrf52/features"),
    ("device_guides/working_with_nrf/nrf52/features","device_guides/nrf52/features"),
    ("device_guides/nrf52/features","app_dev/device_guides/nrf52/features"),
    ("ug_nrf52_gs","device_guides/working_with_nrf/nrf52/gs"),
    ("working_with_nrf/nrf52/gs", "device_guides/working_with_nrf/nrf52/gs"),
    ("device_guides/working_with_nrf/nrf52/gs","gsg_guides/nrf52_gs"),
    ("device_guides/nrf52", "device_guides/nrf52/index"),
    ("device_guides/nrf52/index", "app_dev/device_guides/nrf52/index"),
    ("device_guides/nrf52/building", "app_dev/device_guides/nrf52/building"),
    ("device_guides/nrf52/fota_update", "app_dev/device_guides/nrf52/fota_update"),
    ("ug_nrf53", "device_guides/nrf53"),
    ("nrf53", "device_guides/nrf53"),
    ("device_guides/nrf53", "device_guides/nrf53/index"),
    ("device_guides/nrf53/index", "app_dev/device_guides/nrf53/index"),
    ("ug_nrf5340","device_guides/working_with_nrf/nrf53/nrf5340"),
    ("working_with_nrf/nrf53/nrf5340", "device_guides/working_with_nrf/nrf53/nrf5340"),
    ("device_guides/working_with_nrf/nrf53/nrf5340", "device_guides/nrf53/index"),
    ("device_guides/nrf53/index", "app_dev/device_guides/nrf53/index"),
    ("device_guides/nrf53/building_nrf53", "app_dev/device_guides/nrf53/building_nrf53"),
    ("device_guides/nrf53/features_nrf53", "app_dev/device_guides/nrf53/features_nrf53"),
    ("device_guides/nrf53/fota_update_nrf5340", "app_dev/device_guides/nrf53/fota_update_nrf5340"),
    ("device_guides/nrf53/logging_nrf5340", "app_dev/device_guides/nrf53/logging_nrf5340"),
    ("device_guides/nrf53/multi_image_nrf5340", "app_dev/device_guides/nrf53/multi_image_nrf5340"),
    ("device_guides/working_with_nrf/nrf53/qspi_xip_guid", "device_guides/nrf53/qspi_xip_guide_nrf5340"),
    ("device_guides/nrf53/qspi_xip_guide_nrf5340", "app_dev/device_guides/nrf53/qspi_xip_guide_nrf5340"),
    ("device_guides/nrf53/serial_recovery", "app_dev/device_guides/nrf53/serial_recovery"),
    ("device_guides/nrf53/simultaneous_multi_image_dfu_nrf5340", "app_dev/device_guides/nrf53/simultaneous_multi_image_dfu_nrf5340"),
    ("device_guides/nrf53/thingy53_application_guide", "app_dev/device_guides/nrf53/thingy53_application_guide"),
    ("device_guides/working_with_nrf/nrf53/nrf5340_gs","gsg_guides/nrf5340_gs"),
    ("ug_thingy53","device_guides/working_with_nrf/nrf53/thingy53"),
    ("working_with_nrf/nrf53/thingy53","device_guides/working_with_nrf/nrf53/thingy53"),
    ("device_guides/working_with_nrf/nrf53/thingy53", "device_guides/nrf53/index"),
    ("ug_thingy53_gs","device_guides/working_with_nrf/nrf53/thingy53_gs"),
    ("working_with_nrf/nrf53/thingy53_gs", "device_guides/working_with_nrf/nrf53/thingy53_gs"),
    ("device_guides/working_with_nrf/nrf53/thingy53_gs","gsg_guides/thingy53_gs"),
    ("ug_nrf7002_constrained","device_guides/working_with_nrf/nrf70/developing/constrained"),
    ("working_with_nrf/nrf70/developing/constrained","device_guides/working_with_nrf/nrf70/developing/constrained"),
    ("device_guides/working_with_nrf/nrf70/developing/constrained","device_guides/nrf70/constrained"),
    ("device_guides/nrf70/constrained","app_dev/device_guides/nrf70/constrained"),
    ("ug_nrf70_developing","device_guides/working_with_nrf/nrf70/developing/index"),
    ("working_with_nrf/nrf70/developing/index","device_guides/working_with_nrf/nrf70/developing/index"),
    ("device_guides/working_with_nrf/nrf70/developing/index","device_guides/nrf70/index"),
    ("device_guides/nrf70/index","app_dev/device_guides/nrf70/index"),
    ("ug_nrf70_developing_powersave","device_guides/working_with_nrf/nrf70/developing/powersave"),
    ("working_with_nrf/nrf70/developing/powersave","device_guides/working_with_nrf/nrf70/developing/powersave"),
    ("device_guides/working_with_nrf/nrf70/developing/powersave","protocols/wifi/powersave"),
    ("protocols/wifi/powersave","protocols/wifi/station_mode/powersave"),
    ("ug_nrf70_features","device_guides/working_with_nrf/nrf70/features"),
    ("working_with_nrf/nrf70/features","device_guides/working_with_nrf/nrf70/features"),
    ("device_guides/working_with_nrf/nrf70/features","device_guides/nrf70/features"),
    ("device_guides/nrf70/features","app_dev/device_guides/nrf70/features"),
    ("ug_nrf7002_gs","device_guides/working_with_nrf/nrf70/gs"),
    ("working_with_nrf/nrf70/gs", "device_guides/working_with_nrf/nrf70/gs"),
    ("device_guides/working_with_nrf/nrf70/gs","gsg_guides/nrf7002_gs"),
    ("device_guides/working_with_nrf/nrf70/developing/debugging","protocols/wifi/debugging"),
    ("device_guides/working_with_nrf/nrf70/developing/raw_tx_operation","protocols/wifi/raw_tx_operation"),
    ("protocols/wifi/raw_tx_operation","protocols/wifi/advanced_modes/raw_tx_operation"),
    ("device_guides/working_with_nrf/nrf70/developing/regulatory_support","protocols/wifi/regulatory_support"),
    ("device_guides/working_with_nrf/nrf70/developing/sap","protocols/wifi/sap"),
    ("device_guides/working_with_nrf/nrf70/developing/scan_operation","protocols/wifi/scan_operation"),
    ("protocols/wifi/scan_operation","protocols/wifi/scan_mode/scan_operation"),
    ("device_guides/working_with_nrf/nrf70/developing/fw_patches_ext_flash","device_guides/nrf70/fw_patches_ext_flash"),
    ("device_guides/nrf70/fw_patches_ext_flash","app_dev/device_guides/nrf70/fw_patches_ext_flash"),
    ("device_guides/working_with_nrf/nrf70/developing/nrf70_fw_patch_update","device_guides/nrf70/nrf70_fw_patch_update"),
    ("device_guides/nrf70/nrf70_fw_patch_update","app_dev/device_guides/nrf70/nrf70_fw_patch_update"),
    ("device_guides/working_with_nrf/nrf70/developing/power_profiling","device_guides/nrf70/power_profiling"),
    ("device_guides/nrf70/power_profiling","app_dev/device_guides/nrf70/power_profiling"),
    ("device_guides/working_with_nrf/nrf70/developing/stack_partitioning","device_guides/nrf70/stack_partitioning"),
    ("device_guides/nrf70/stack_partitioning","app_dev/device_guides/nrf70/stack_partitioning"),
    ("device_guides/working_with_nrf/nrf70/nrf7002eb_gs","device_guides/nrf70/nrf7002eb_dev_guide"),
    ("device_guides/nrf70/nrf7002eb_dev_guide","app_dev/device_guides/nrf70/nrf7002eb_dev_guide"),
    ("device_guides/working_with_nrf/nrf70/nrf7002ek_gs","device_guides/nrf70/nrf7002ek_dev_guide"),
    ("device_guides/nrf70/nrf7002ek_dev_guide","app_dev/device_guides/nrf70/nrf7002ek_dev_guide"),
    ("protocols/wifi/sniffer_rx_operation","protocols/wifi/advanced_modes/sniffer_rx_operation"),
    ("ug_nrf9160","device_guides/working_with_nrf/nrf91/nrf9160"),
    ("working_with_nrf/nrf91/nrf9160","device_guides/working_with_nrf/nrf91/nrf9160"),
    ("device_guides/working_with_nrf/nrf91/nrf9160", "device_guides/nrf91/index"),
    ("device_guides/nrf91/index","app_dev/device_guides/nrf91/index"),
    ("ug_nrf9161","device_guides/working_with_nrf/nrf91/nrf9161"),
    ("working_with_nrf/nrf91/nrf9161","device_guides/working_with_nrf/nrf91/nrf9161"),
    ("device_guides/working_with_nrf/nrf91/nrf9161", "device_guides/nrf91/index"),
    ("device_guides/nrf91/index","app_dev/device_guides/nrf91/index"),
    ("ug_nrf9160_gs","device_guides/working_with_nrf/nrf91/nrf9160_gs"),
    ("working_with_nrf/nrf91/nrf9160_gs","device_guides/working_with_nrf/nrf91/nrf9160_gs"),
    ("device_guides/working_with_nrf/nrf91/nrf9160_gs", "gsg_guides/nrf9160_gs"),
    ("ug_nrf91_features","device_guides/working_with_nrf/nrf91/nrf91_features"),
    ("working_with_nrf/nrf91/nrf91_features","device_guides/working_with_nrf/nrf91/nrf91_features"),
    ("device_guides/working_with_nrf/nrf91/nrf91_features", "device_guides/nrf91/nrf91_features"),
    ("device_guides/nrf91/nrf91_features","app_dev/device_guides/nrf91/nrf91_features"),
    ("ug_thingy91","device_guides/working_with_nrf/nrf91/thingy91"),
    ("working_with_nrf/nrf91/thingy91","device_guides/working_with_nrf/nrf91/thingy91"),
    ("device_guides/working_with_nrf/nrf91/thingy91", "device_guides/nrf91/index"),
    ("device_guides/nrf91/index","app_dev/device_guides/nrf91/index"),
    ("ug_thingy91_gsg","device_guides/working_with_nrf/nrf91/thingy91_gsg"),
    ("working_with_nrf/nrf91/thingy91_gsg","device_guides/working_with_nrf/nrf91/thingy91_gsg"),
    ("device_guides/working_with_nrf/nrf91/thingy91_gsg", "gsg_guides/thingy91_gsg"),
    ("device_guides/working_with_nrf/nrf91/nrf91_snippet", "device_guides/nrf91/nrf91_snippet"),
    ("device_guides/nrf91/nrf91_snippet","app_dev/device_guides/nrf91/nrf91_snippet"),
    ("device_guides/nrf91/nrf91_board_controllers","app_dev/device_guides/nrf91/nrf91_board_controllers"),
    ("device_guides/nrf91/nrf91_building","app_dev/device_guides/nrf91/nrf91_building"),
    ("device_guides/nrf91/nrf91_cloud_certificate","app_dev/device_guides/nrf91/nrf91_cloud_certificate"),
    ("device_guides/nrf91/nrf91_dk_updating_fw_programmer","app_dev/device_guides/nrf91/nrf91_dk_updating_fw_programmer"),
    ("device_guides/nrf91/nrf91_programming","app_dev/device_guides/nrf91/nrf91_programming"),
    ("device_guides/nrf91/nrf91_testing_at_client","app_dev/device_guides/nrf91/nrf91_testing_at_client"),
    ("device_guides/nrf91/nrf91_updating_fw_programmer","app_dev/device_guides/nrf91/nrf91_updating_fw_programmer"),
    ("device_guides/nrf91/nrf9160_external_flash","app_dev/device_guides/nrf91/nrf9160_external_flash"),
    ("device_guides/nrf91/thingy91_connecting","app_dev/device_guides/nrf91/thingy91_connecting"),
    ("device_guides/nrf91/thingy91_updating_fw_programmer","app_dev/device_guides/nrf91/thingy91_updating_fw_programmer"),
    ("device_guides/pmic","device_guides/pmic/index"),
    ("device_guides/pmic/index","app_dev/device_guides/pmic/index"),
    ("device_guides/working_with_pmic/npm1300/developing","device_guides/pmic/npm1300"),
    ("device_guides/working_with_pmic/npm1300/features","device_guides/pmic/npm1300"),
    ("device_guides/working_with_pmic/npm1300/gs","device_guides/pmic/npm1300"),
    ("device_guides/pmic/npm1300","app_dev/device_guides/pmic/npm1300"),
    ("device_guides/nrf54h","app_dev/device_guides/nrf54h"),
    ("device_guides/working_with_nrf/nrf54h/ug_nrf54h20_app_samples","app_dev/device_guides/working_with_nrf/nrf54h/ug_nrf54h20_app_samples"),
    ("device_guides/working_with_nrf/nrf54h/ug_nrf54h20_architecture","app_dev/device_guides/working_with_nrf/nrf54h/ug_nrf54h20_architecture"),
    ("device_guides/working_with_nrf/nrf54h/ug_nrf54h20_architecture_boot","app_dev/device_guides/working_with_nrf/nrf54h/ug_nrf54h20_architecture_boot"),
    ("device_guides/working_with_nrf/nrf54h/ug_nrf54h20_architecture_cpu","app_dev/device_guides/working_with_nrf/nrf54h/ug_nrf54h20_architecture_cpu"),
    ("device_guides/working_with_nrf/nrf54h/ug_nrf54h20_architecture_ipc","app_dev/device_guides/working_with_nrf/nrf54h/ug_nrf54h20_architecture_ipc"),
    ("device_guides/working_with_nrf/nrf54h/ug_nrf54h20_architecture_lifecycle","app_dev/device_guides/working_with_nrf/nrf54h/ug_nrf54h20_architecture_lifecycle"),
    ("device_guides/working_with_nrf/nrf54h/ug_nrf54h20_architecture_memory","app_dev/device_guides/working_with_nrf/nrf54h/ug_nrf54h20_architecture_memory"),
    ("device_guides/working_with_nrf/nrf54h/ug_nrf54h20_configuration","app_dev/device_guides/working_with_nrf/nrf54h/ug_nrf54h20_configuration"),
    ("device_guides/working_with_nrf/nrf54h/ug_nrf54h20_custom_pcb","app_dev/device_guides/working_with_nrf/nrf54h/ug_nrf54h20_custom_pcb"),
    ("device_guides/working_with_nrf/nrf54h/ug_nrf54h20_debugging","app_dev/device_guides/working_with_nrf/nrf54h/ug_nrf54h20_debugging"),
    ("device_guides/working_with_nrf/nrf54h/ug_nrf54h20_gs","app_dev/device_guides/working_with_nrf/nrf54h/ug_nrf54h20_gs"),
    ("device_guides/working_with_nrf/nrf54h/ug_nrf54h20_logging","app_dev/device_guides/working_with_nrf/nrf54h/ug_nrf54h20_logging"),
    ("device_guides/working_with_nrf/nrf54h/ug_nrf54h20_nrf7002ek","app_dev/device_guides/working_with_nrf/nrf54h/ug_nrf54h20_nrf7002ek"),
    ("device_guides/working_with_nrf/nrf54h/ug_nrf54h20_suit_compare_other_dfu","app_dev/device_guides/working_with_nrf/nrf54h/ug_nrf54h20_suit_compare_other_dfu"),
    ("device_guides/working_with_nrf/nrf54h/ug_nrf54h20_suit_components","app_dev/device_guides/working_with_nrf/nrf54h/ug_nrf54h20_suit_components"),
    ("device_guides/working_with_nrf/nrf54h/ug_nrf54h20_suit_customize_dfu","app_dev/device_guides/working_with_nrf/nrf54h/ug_nrf54h20_suit_customize_dfu"),
    ("device_guides/working_with_nrf/nrf54h/ug_nrf54h20_suit_customize_qsg","app_dev/device_guides/working_with_nrf/nrf54h/ug_nrf54h20_suit_customize_qsg"),
    ("device_guides/working_with_nrf/nrf54h/ug_nrf54h20_suit_dfu","app_dev/device_guides/working_with_nrf/nrf54h/ug_nrf54h20_suit_dfu"),
    ("device_guides/working_with_nrf/nrf54h/ug_nrf54h20_suit_external_memory","app_dev/device_guides/working_with_nrf/nrf54h/ug_nrf54h20_suit_external_memory"),
    ("device_guides/working_with_nrf/nrf54h/ug_nrf54h20_suit_fetch","app_dev/device_guides/working_with_nrf/nrf54h/ug_nrf54h20_suit_fetch"),
    ("device_guides/working_with_nrf/nrf54h/ug_nrf54h20_suit_hierarchical_manifests","app_dev/device_guides/working_with_nrf/nrf54h/ug_nrf54h20_suit_hierarchical_manifests"),
    ("device_guides/working_with_nrf/nrf54h/ug_nrf54h20_suit_intro","app_dev/device_guides/working_with_nrf/nrf54h/ug_nrf54h20_suit_intro"),
    ("device_guides/working_with_nrf/nrf54h/ug_nrf54h20_suit_manifest_overview","app_dev/device_guides/working_with_nrf/nrf54h/ug_nrf54h20_suit_manifest_overview"),
    ("device_guides/nrf54l","app_dev/device_guides/nrf54l"),
    ("device_guides/working_with_nrf/nrf54l/features","app_dev/device_guides/working_with_nrf/nrf54l/features"),
    ("device_guides/working_with_nrf/nrf54l/nrf54l15_gs","app_dev/device_guides/working_with_nrf/nrf54l/nrf54l15_gs"),
    ("device_guides/working_with_nrf/nrf54l/peripheral_sensor_node_shield","app_dev/device_guides/working_with_nrf/nrf54l/peripheral_sensor_node_shield"),
    ("device_guides/working_with_nrf/nrf54l/testing_dfu","app_dev/device_guides/working_with_nrf/nrf54l/testing_dfu"),
    ("known_issues","releases_and_maturity/known_issues"),
    ("libraries/networking/nrf_cloud_agps","libraries/networking/nrf_cloud_agnss"),
    ("applications/nrf5340_audio/README","applications/nrf5340_audio/index"),
    ("libraries/nrf_security/index", "libraries/security/nrf_security/index"),
    ("libraries/nrf_security/doc/backend_config", "libraries/security/nrf_security/doc/backend_config"),
    ("libraries/nrf_security/doc/configuration", "libraries/security/nrf_security/doc/configuration"),
    ("libraries/nrf_security/doc/driver_config", "libraries/security/nrf_security/doc/driver_config"),
    ("libraries/nrf_security/doc/drivers", "libraries/security/nrf_security/doc/drivers"),
    ("libraries/nrf_security/doc/mbed_tls_header", "libraries/security/nrf_security/doc/mbed_tls_header"),
    ("libraries/bin/bt_ll_acs_nrf53/index", "nrfxlib/softdevice_controller/doc/isochronous_channels"),
    ("libraries/others/fatal_error", "libraries/security/fatal_error"),
    ("libraries/others/flash_patch", "libraries/security/bootloader/flash_patch"),
    ("libraries/others/fprotect", "libraries/security/bootloader/fprotect"),
    ("libraries/others/fw_info", "libraries/security/bootloader/fw_info"),
    ("libraries/others/hw_unique_key", "libraries/security/hw_unique_key"),
    ("libraries/others/identity_key", "libraries/security/identity_key"),
    ("libraries/tfm/index", "libraries/security/tfm/index"),
    ("libraries/tfm/tfm_ioctl_api", "libraries/security/tfm/tfm_ioctl_api"),
    ("libraries/bootloader/bl_crypto", "libraries/security/bootloader/bl_crypto"),
    ("libraries/bootloader/bl_storage", "libraries/security/bootloader/bl_storage"),
    ("libraries/bootloader/bl_validation", "libraries/security/bootloader/bl_validation"),
    ("libraries/bootloader/index", "libraries/security/bootloader/index"),
]
