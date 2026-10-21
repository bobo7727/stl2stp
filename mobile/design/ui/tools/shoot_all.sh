#!/bin/bash
# Renders every key screen to ../screenshots with headless Chrome (2x, iPhone 15 393x852 pt).
cd "$(dirname "$0")"; OUT=../screenshots; mkdir -p $OUT
shot(){ ./shoot.sh "$OUT/$1.png" "bare=1&theme=$2${4:+&$4}" "$3" ${5:-} ${6:-}; echo "$1"; }
L=(
 "01_onboarding_1|onboarding1"
 "02_onboarding_2|onboarding2"
 "03_home_empty|home?empty=1"
 "04_home|home"
 "05_import_help_sheet|home?sheet=help"
 "06_preview|preview"
 "07_preview_options|preview?scroll=330"
 "08_advanced_settings|preview?adv=1&preset=smaller&scroll=600"
 "09_large_model_warning|preview?file=large&scroll=330"
 "10_iges_chinese_meta_warning|preview?fmt=iges&adv=1&meta=zh&scroll=1050"
 "11_converting|converting?p=0.62"
 "12_cancel_confirm|converting?p=0.62&confirm=1"
 "13_done|done"
 "14_share_sheet|done?sheet=share"
 "15_error_bad_file|error?type=format"
 "16_error_too_big|error?type=too_big"
 "17_error_out_of_memory|error?type=memory"
 "18_cancelled|error?type=cancelled"
 "19_history|history"
 "20_settings|settings"
 "21_licenses|licenses"
 "22_paywall_placeholder|paywall"
)
for e in "${L[@]}"; do n=${e%%|*}; r=${e#*|}; shot "${n}_light" light "$r"; done
for n in 01_onboarding_1 04_home 06_preview 08_advanced_settings 11_converting 13_done 14_share_sheet 15_error_bad_file 20_settings; do
  for e in "${L[@]}"; do [ "${e%%|*}" = "$n" ] && shot "${n}_dark" dark "${e#*|}"; done; done
shot "23_preview_dynamic_type_xl_light" light "preview" "text=xl"
shot "24_ipad_split_light" light "ipad" "device=ipad" 1180 820
shot "24_ipad_split_dark" dark "ipad" "device=ipad" 1180 820
shot "25_preview_en_light" light "preview?scroll=330" "lang=en"
