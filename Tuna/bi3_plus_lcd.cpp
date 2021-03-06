#include <tuna.h>

#include "bi3_plus_lcd.h"

#include "language.h"
#include "cardreader.h"
#include "thermal/thermal.hpp"
#include "stepper.h"
#include "configuration_store.h"
#include "utility.h"
#include "watchdog.h"

#if ENABLED(PRINTCOUNTER)
#include "printcounter.h"
#include "duration_t.h"
#endif

#include "Tuna_VM.hpp"

using namespace Tuna;

namespace Tuna::lcd
{
	namespace
	{
		enum class OpMode : uint8
		{
			None = 0,
			Level_Init,
			Load_Filament,
			Unload_Filament,
			Move,
			Auto_PID
		};

		inline void read_data();
		void write_statistics();

    chrono::time_ms<uint16> opTime = 0;
    chrono::duration_ms<uint16> opDuration = 0;
    chrono::time_ms<uint16> lcdUpdateTime = 0;
    chrono::duration_ms<uint16> lcdUpdateDuration = 0;
    constexpr const auto lcdUpdatePeriod = 100_ms16;

		uint16 fileIndex = 0;
		OpMode opMode = OpMode::None;
		uint8 tempGraphUpdate = 0;
		Page currentPage = Page::Main_Menu;
		Page lastPage = Page::Main_Menu; // main menu

		void execute_looped_operation(arg_type<chrono::time_ms<uint16>> ms)
		{
      if (__likely(!opTime.elapsed(ms, opDuration)))
      {
        return;
      }

			switch (opMode)
			{
			case OpMode::Level_Init:
			{
				if (axis_homed[X_AXIS] & axis_homed[Y_AXIS] & axis_homed[Z_AXIS]) //stuck if levelling problem?
				{
					opMode = OpMode::None;
					show_page(Page::Level2);//level 2 menu
				}
				else
				{
          opTime = ms;
          opDuration = 200_ms16;
				}
			} break;
			case OpMode::Unload_Filament:
			{
				if (Temperature::current_temperature >= (Temperature::target_temperature - 10_u16))
				{
					enqueue_and_echo_commands("G1 E-1 F120"_p);
				}
        opTime = ms;
        opDuration = 500_ms16;
			} break;
			case OpMode::Load_Filament:
			{
				if (Temperature::current_temperature >= (Temperature::target_temperature - 10_u16))
				{
					enqueue_and_echo_commands("G1 E1 F120"_p);
				}
        opTime = ms;
        opDuration = 500_ms16;
			} break;
			}
		}

		void status_update(arg_type<chrono::time_ms<uint16>> ms)
		{
      const auto elapsedPair = lcdUpdateTime.elapsed_over(ms, lcdUpdateDuration);
      if (__likely(!elapsedPair))
      {
        return;
      }

      lcdUpdateTime = ms;
      lcdUpdateDuration = lcdUpdatePeriod - min(lcdUpdatePeriod, elapsedPair.second);

      const uint16 target_hotend_temperature = Temperature::target_temperature.rounded_to<uint16>();
			const uint16 hotend_temperature = Temperature::degHotend().rounded_to<uint16>();

			const uint16 target_bed_temperature = Temperature::target_temperature_bed.rounded_to<uint16>();
			const uint16 bed_temperature = Temperature::degBed().rounded_to<uint16>();

			const uint8 fan_speed = uint16(fanSpeeds[0] * 100_u16) / 255_u8;

			const auto card_progress = card.percentDone();

			const uint8 buffer[18] = {
				0x5A,
				0xA5,
				0x0F, //data length
				0x82, //write data to sram
				0x00, //starting at 0 vp
				0x00,
				hi(target_hotend_temperature), //0x00 target extruder temp
				lo(target_hotend_temperature),
				hi(hotend_temperature), //0x01 extruder temp
				lo(hotend_temperature),
				hi(target_bed_temperature), //0x02 target bed temp
				lo(target_bed_temperature),
				hi(bed_temperature), //0x03 target bed temp
				lo(bed_temperature),
				0, //0x04 fan speed
				fan_speed,
				0x00, //0x05 card progress
				card.percentDone()
			};

			serial<2>::write(buffer);

			switch (tempGraphUpdate)
			{
			case 2:
				tempGraphUpdate = 1;
				update_graph();
				break;
			default:
				tempGraphUpdate = 2;
				break;
			case 0:
				break;
			}
		}

		//show page OK
		Page get_current_page()
		{
			return currentPage;
		}

		//receive data from lcd OK
		void read_data()
		{
			if (!serial<2>::available(1))
			{
				return;
			}

			if (serial<2>::read() != 0x5A)
			{
				return;
			}

			while (!serial<2>::available(1)) {}

			if (serial<2>::read() != 0xA5)
			{
				return;
			}

			while (!serial<2>::available(3)) {}

			serial<2>::read(); // data length
			serial<2>::read(); // command

			if (serial<2>::read() != 4) // VP MSB
			{
				return;
			}

			while (!serial<2>::available(4)) {}

			const uint8 lcdCommand = serial<2>::read(); // VP LSB
			serial<2>::read();// LEN ?
			serial<2>::read(); //KEY VALUE MSB
			const uint8 lcdData = serial<2>::read(); //KEY VALUE LSB

			switch (lcdCommand)
			{
			case 0x32: {//SD list navigation up/down OK
				if (card.sdprinting)
				{
					show_page(Page::Print); //show print menu
				}
				else
				{
					uint16 fileCnt = 0;
					if (lcdData == 0)
					{
						card.initsd();
						if (__likely(card.cardOK))
						{
							fileCnt = card.getnrfilenames();
							fileIndex = max(fileCnt, 1_u16) - 1;
						}
					}

					if (__likely(card.cardOK))
					{
						fileCnt = fileCnt ? fileCnt : card.getnrfilenames();
						card.getWorkDirName();//??

						if (fileCnt > 5)
						{
							if (lcdData == 1) //UP
							{
								if ((fileIndex + 5) < fileCnt)
								{
									fileIndex += 5;
								}
							}
							else if (lcdData == 2) //DOWN
							{
								if (fileIndex >= 5)
								{
									fileIndex -= 5;
								}
							}
						}

						{
							constexpr const uint8 buffer[6] = {
								0x5A,
								0xA5,
								0x9F,
								0x82,
								0x01,
								0x00
							};
							serial<2>::write(buffer);
						}

						for (uint8 i = 0; i < 6; ++i)
						{
							uint8 buffer[26];
							card.getfilename(fileIndex - i);
							serial<2>::write(card.longFilename, 26); // TODO why 26? No '\0'?
						}

						show_page(Page::SD_Card); //show sd card menu
					}
				}
				break;
			}
			case 0x33: {//FILE SELECT OK
				if (card.cardOK) {
					if (((fileIndex + 10) - lcdData) >= 10)
					{
						card.getfilename(fileIndex - lcdData);

						constexpr const uint8 buffer[6] = {
							0x5A,
							0xA5,
							0x1D,
							0x82,
							0x01,
							0x4E
						};
						serial<2>::write(buffer);
						serial<2>::write(card.longFilename, 26);

						card.openFile(card.filename, true);
						card.startFileprint();
						print_job_timer.start();

						tempGraphUpdate = 2;

						show_page(Page::Print);//print menu
					}
				}
				break;
			}
			case 0x35: {//print stop OK
				card.stopSDPrint();
				clear_command_queue();
				quickstop_stepper();
				print_job_timer.stop();
				Temperature::disable_all_heaters();
#if FAN_COUNT > 0
				for (uint8 i = 0; i < FAN_COUNT; ++i)
				{
					fanSpeeds[i] = 0;
				}
#endif
				tempGraphUpdate = 0;
				show_page(Page::Main_Menu); //main menu
				break;
			}
			case 0x36: {//print pause OK
				card.pauseSDPrint();
				print_job_timer.pause();
#if ENABLED(PARK_HEAD_ON_PAUSE)
				enqueue_and_echo_commands("M125"_p);
#endif
				break;
			}
			case 0x37: {//print start OK
#if ENABLED(PARK_HEAD_ON_PAUSE)
				enqueue_and_echo_commands("M24"_p);
#else
				card.startFileprint();
				print_job_timer.start();
#endif
				break;
			}
			case 0x3C: { //Preheat options
				if (lcdData == 0) {
					//Serial.println(thermalManager.target_temperature[0]);
					//writing preset temps to lcd

					const uint16 preset_hotend[3] = {
						Planner::preheat_presets[0].hotend,
						Planner::preheat_presets[1].hotend,
						Planner::preheat_presets[2].hotend
					};
					const uint8 preset_bed[3] = {
						Planner::preheat_presets[0].bed,
						Planner::preheat_presets[1].bed,
						Planner::preheat_presets[2].bed
					};

					const uint8 buffer[18] = {
						 0x5A,
						 0xA5,
						 0x0F, //data length
						 0x82, //write data to sram
						 0x05, //starting at 0x0570 vp
						 0x70,
						 hi(preset_hotend[0]),
						 lo(preset_hotend[0]),
						 0x00,
						 preset_bed[0],
						 hi(preset_hotend[1]),
						 lo(preset_hotend[1]),
						 0x00,
						 preset_bed[1],
						 hi(preset_hotend[2]),
						 lo(preset_hotend[2]),
						 0x00,
						 preset_bed[2],
					};

					serial<2>::write(buffer);

					show_page(Page::Preheat);//open preheat screen
									//Serial.println(thermalManager.target_temperature[0]);
				}
				else {
					//Serial.println(thermalManager.target_temperature[0]);
					//read presets

					{
						constexpr const uint8 buffer[7] = {
							0x5A,
							0xA5,
							0x04, //data length
							0x83, //read sram
							0x05, //vp 0570
							0x70,
							0x06, //length
						};

						serial<2>::write(buffer);
					}

					//read user entered values from sram
					uint8 buffer[19];
					uint8 bytesRead = serial<2>::read_bytes(buffer);
					if ((bytesRead != 19) | (buffer[0] != 0x5A) | (buffer[1] != 0xA5))
					{
						break;
					}
					Planner::preheat_presets[0].hotend = uint16{ buffer[7] } * 256_i16 + buffer[8];
					Planner::preheat_presets[0].bed = (uint8)buffer[10];
					Planner::preheat_presets[1].hotend = uint16{ buffer[11] } * 256_i16 + buffer[12];
					Planner::preheat_presets[1].bed = uint8{ buffer[14] };
					Planner::preheat_presets[2].hotend = uint16{ buffer[15] } * 256_i16 + buffer[16];
					Planner::preheat_presets[2].bed = uint8{ buffer[18] };
					enqueue_and_echo_commands("M500"_p);

					char command[20];
					const uint8 idx = lcdData - 1;
					sprintf_P(command, "M104 S%u"_p.c_str(), Planner::preheat_presets[idx].hotend); //build heat up command (extruder)
					enqueue_and_echo_command(command); //enque heat command
					sprintf_P(command, "M140 S%u"_p.c_str(), Planner::preheat_presets[idx].bed); //build heat up command (bed)
					enqueue_and_echo_command(command); //enque heat command
				}
			}
			case 0x34: {//cool down OK
				Temperature::disable_all_heaters();
				break;
			}
			case 0x3E: {//send pid/motor config to lcd OK

				const uint16 axis_steps_mm[4] = {
					round<uint16>(planner.axis_steps_per_mm[X_AXIS] * 10.0f),
					round<uint16>(planner.axis_steps_per_mm[Y_AXIS] * 10.0f),
					round<uint16>(planner.axis_steps_per_mm[Z_AXIS] * 10.0f),
					round<uint16>(planner.axis_steps_per_mm[E_AXIS] * 10.0f),
				};

        const uint16 Kp = 0;// uint16{ PID_PARAM(Kp) * 10.0f };
        const uint16 Ki = 0;// uint16{ unscalePID_i(PID_PARAM(Ki)) * 10.0f };
        const uint16 Kd = 0;// uint16{ unscalePID_d(PID_PARAM(Kd)) * 10.0f };

				const uint8 buffer[20] = {
					0x5A,
					0xA5,
					0x11,
					0x82,
					0x03,
					0x24,
					hi(axis_steps_mm[0]),
					lo(axis_steps_mm[0]),
					hi(axis_steps_mm[1]),
					lo(axis_steps_mm[1]),
					hi(axis_steps_mm[2]),
					lo(axis_steps_mm[2]),
					hi(axis_steps_mm[3]),
					lo(axis_steps_mm[3]),
					hi(Kp),
					lo(Kp),
					hi(Ki),
					lo(Ki),
					hi(Kd),
					lo(Kd),
				};

				serial<2>::write(buffer);

				show_page(lcdData ? Page::PID : Page::Motor); //show pid screen or motor screen
				break;
			}
			case 0x3F: {//save pid/motor config OK
				{
					constexpr const uint8 buffer[7] = {
						0x5A,
						0xA5,
						0x04,
						0x83,
						0x03,
						0x24,
						0x07
					};

					serial<2>::write(buffer);
				}

				uint8 buffer[21];
				uint8 bytesRead = serial<2>::read_bytes(buffer);
				if ((bytesRead != 21) | (buffer[0] != 0x5A) | (buffer[1] != 0xA5)) {
					break;
				}
				planner.axis_steps_per_mm[X_AXIS] = float( (uint16((uint16)buffer[7] * 256) + buffer[8]) ) * 0.1f;
				//Serial.println(lcdBuff[7]);
				//Serial.println(lcdBuff[8]);
				//Serial.println(lcdBuff[9]);
				//Serial.println(lcdBuff[10]);
				planner.axis_steps_per_mm[Y_AXIS] = float( (uint16((uint16)buffer[9] * 256) + buffer[10]) ) * 0.1f;
				planner.axis_steps_per_mm[Z_AXIS] = float( (uint16((uint16)buffer[11] * 256) + buffer[12]) ) * 0.1f;
				planner.axis_steps_per_mm[E_AXIS] = float( (uint16((uint16)buffer[13] * 256) + buffer[14]) ) * 0.1f;

				//PID_PARAM(Kp) = float{ ((uint16)buffer[15] * 256 + buffer[16]) } * 0.1f;
				//PID_PARAM(Ki) = scalePID_i(float{ ((uint16)buffer[17] * 256 + buffer[18]) } * 0.1f);
				//PID_PARAM(Kd) = scalePID_d(float{ ((uint16)buffer[19] * 256 + buffer[20]) } * 0.1f);

				enqueue_and_echo_commands("M500"_p);
				show_page(Page::System_Menu);//show system menu
				break;
			}
			case 0x42: {//factory reset OK
				enqueue_and_echo_commands("M502"_p);
				enqueue_and_echo_commands("M500"_p);
				break;
			}
			case 0x47: {//print config open OK
				const uint16 hotend_target = uint16(Temperature::degTargetHotend());
				const uint16 bed_target = uint16(Temperature::degTargetBed());
				const uint8 fan_speed = (fanSpeeds[0] * 100) / 256;

				const uint8 buffer[14] = {
					0x5A,
					0xA5,
					0x0B,
					0x82,
					0x03,
					0x2B,
					hi(feedrate_percentage), //0x2B
					lo(feedrate_percentage),
					hi(hotend_target), //0x2C
					lo(hotend_target),
					hi(bed_target), //0x2D
					lo(bed_target),
					0,//0x2E
					fan_speed
				};

				serial<2>::write(buffer);

				show_page(Page::Print_Config);//print config
				break;
			}
			case 0x40: {//print config save OK
				{
					constexpr const uint8 buffer[7] = {
						0x5A,
						0xA5,
						0x04,//4 byte
						0x83,//command
						0x03,// start addr
						0x2B,
						0x04, //4 vp
					};

					serial<2>::write(buffer);
				}

				uint8 buffer[15];
				uint8 bytesRead = serial<2>::read_bytes(buffer);
				if ((bytesRead != 15) | (buffer[0] != 0x5A) | (buffer[1] != 0xA5)) {
					break;
				}
				feedrate_percentage = (uint16)buffer[7] * 256 + buffer[8];
				Temperature::setTargetHotend((uint16)buffer[9] * 256 + buffer[10]);

				Temperature::setTargetBed(buffer[12]);
				fanSpeeds[0] = (uint16)buffer[14] * 256 / 100;
				show_page(Page::Print);// show print menu
				break;
			}
			case 0x4A: {//load/unload filament back OK
				opMode = OpMode::None;
				clear_command_queue();
				enqueue_and_echo_commands("G90"_p); // absolute mode
				Temperature::setTargetHotend(0);
				show_page(Page::Filament);//filament menu
				break;
			}
			case 0x4C: {//level menu OK
				switch (lcdData)
				{
				case 0: {
					show_page(Page::Level1); //level 1
					axis_homed[X_AXIS] = axis_homed[Y_AXIS] = axis_homed[Z_AXIS] = false;
					//enqueue_and_echo_commands("G90"_p); //absolute mode
					enqueue_and_echo_commands("G28"_p);//homeing
          opTime = chrono::time_ms<uint16>::get();
          opDuration = 200_ms16;
					opMode = OpMode::Level_Init;
				} break;
				case 1: { //fl
					enqueue_and_echo_commands("G6 Z10"_p);
					enqueue_and_echo_commands("G6 X35 Y35"_p);
					enqueue_and_echo_commands("G6 Z0"_p);
				} break;
				case 2: { //rr
					enqueue_and_echo_commands("G6 Z10"_p);
					enqueue_and_echo_commands("G6 X165 Y170"_p);
					enqueue_and_echo_commands("G6 Z0"_p);
				} break;
				case 3: { //fr
					enqueue_and_echo_commands("G6 Z10"_p);
					enqueue_and_echo_commands("G6 X165 Y35"_p);
					enqueue_and_echo_commands("G6 Z0"_p);
				} break;
				case 4: { //rl
					enqueue_and_echo_commands("G6 Z10"_p);
					enqueue_and_echo_commands("G6 X35 Y165"_p);
					enqueue_and_echo_commands("G6 Z0"_p);
				} break;
				case 5: { //c
					enqueue_and_echo_commands("G6 Z10"_p);
					enqueue_and_echo_commands("G6 X100 Y100"_p);
					enqueue_and_echo_commands("G6 Z0"_p);
				} break;
				case 6: { //back
					enqueue_and_echo_commands("G6 Z30"_p);
					show_page(Page::Tool_Menu); //tool menu
				} break;
				}
				break;
			}

			case 0x51: { //load_unload_menu
				switch (lcdData)
				{
				case 0: {
					//writing default temp to lcd
					constexpr const uint8 buffer[8] = {
						0x5A,
						0xA5,
						0x05, //data length
						0x82, //write data to sram
						0x05, //starting at 0x0500 vp
						0x20,
						0x00,
						0xC8 //extruder temp (200)
					};
					serial<2>::write(buffer);

					show_page(Page::Filament);//open load/unload_menu
				} break;
				case 1:
				case 2: {
					//read bed/hotend temp
					{
						constexpr const uint8 buffer[7] = {
							0x5A,
							0xA5,
							0x04, //data length
							0x83, //read sram
							0x05, //vp 0520
							0x20,
							0x01 //length
						};

						serial<2>::write(buffer);
					}

					//read user entered values from sram
					uint8 buffer[9];
					uint8 bytesRead = serial<2>::read_bytes(buffer);
					if ((bytesRead != 9) | (buffer[0] != 0x5A) | (buffer[1] != 0xA5)) {
						break;
					}
					int16 hotendTemp = (int16)buffer[7] * 256 + buffer[8];
					Temperature::setTargetHotend(hotendTemp);
					enqueue_and_echo_commands("G91"_p); // relative mode
          opTime = chrono::time_ms<uint16>::get();
          opDuration = 500_ms16;
					if (lcdData == 1) {
						opMode = OpMode::Load_Filament;
					}
					else if (lcdData == 2) {
						opMode = OpMode::Unload_Filament;
					}
				} break;
				}
				break;
			}
			case 0x00: {
				clear_command_queue();
				enqueue_and_echo_commands("G8 X5"_p);
				break;
			}
			case 0x01: {
				clear_command_queue();
				enqueue_and_echo_commands("G8 X-5"_p);

				break;
			}
			case 0x02: {
				clear_command_queue();
				enqueue_and_echo_commands("G8 Y5"_p);

				break;
			}
			case 0x03: {
				clear_command_queue();
				enqueue_and_echo_commands("G8 Y-5"_p);

				break;
			}
			case 0x04: {
				clear_command_queue();
				enqueue_and_echo_commands("G8 Z2"_p);

				break;
			}
			case 0x05: {
				clear_command_queue();
				enqueue_and_echo_commands("G8 Z-2"_p);

				break;
			}
			case 0x06: {
				if (!Temperature::is_coldextrude()) {
					clear_command_queue();
					enqueue_and_echo_commands("G14 E1 F120"_p);
				}
				break;
			}
			case 0x07: {
				if (!Temperature::is_coldextrude()) {
					clear_command_queue();
					enqueue_and_echo_commands("G14 E-1 F120"_p);
				}
				break;
			}
			case 0x54: {//disable motors OK!!!
				enqueue_and_echo_commands("M84"_p);
				axis_homed[X_AXIS] = axis_homed[Y_AXIS] = axis_homed[Z_AXIS] = false;
				break;
			}
			case 0x43: {//home x OK!!!
				enqueue_and_echo_commands("G28 X0"_p);
				break;
			}
			case 0x44: {//home y OK!!!
				enqueue_and_echo_commands("G28 Y0"_p);
				break;
			}
			case 0x45: {//home z OK!!!
				enqueue_and_echo_commands("G28 Z0"_p);
				break;
			}
			case 0x1C: {//home xyz OK!!!
				enqueue_and_echo_commands("G28"_p);
				break;
			}
			case 0x5B: { //stats menu
						 //sending stats to lcd
				write_statistics();

				show_page(Page::Statistics);//open stats screen on lcd
				break;
			}
			case 0x5C: { //auto pid menu

				if (lcdData == 0) {
					//writing default temp to lcd
					constexpr const uint8 buffer[8] = {
						0x5A,
						0xA5,
						0x05, //data length
						0x82, //write data to sram
						0x05, //starting at 0x0500 vp
						0x20,
						0x00,
						0xC8, //extruder temp (200)
					};
					serial<2>::write(buffer);

					show_page(Page::Auto_PID);//open auto pid screen
				}
				else if (lcdData == 1) { //auto pid start button pressed (1=hotend,2=bed)
										 //read bed/hotend temp

					{
						constexpr const uint8 buffer[7] = {
							0x5A,
							0xA5,
							0x04, //data length
							0x83, //read sram
							0x05, //vp 0520
							0x20,
							0x01, //length
						};
						serial<2>::write(buffer);
					}

					uint8 buffer[9];
					//read user entered values from sram
					uint8 bytesRead = serial<2>::read_bytes(buffer);
					if ((bytesRead != 9) | (buffer[0] != 0x5A) | (buffer[1] != 0xA5)) {
						break;
					}
					uint16 hotendTemp = (uint16)buffer[7] * 256 + buffer[8];
					//Serial.println(hotendTemp);
					char command[30];
					sprintf_P(command, "M303 S%d E0 C8 U1"_p.c_str(), hotendTemp); //build auto pid command (extruder)
					enqueue_and_echo_commands("M106"_p); //Turn on fan
					enqueue_and_echo_command(command); //enque pid command
					tempGraphUpdate = 2;
				}
			} break;
			case 0x3D: { //Close temp screen
				if (lcdData == 1) // back
				{
					tempGraphUpdate = 0;
					//Serial.println(uint8(lastPage));
					show_page(lastPage);
				}
				else // open temp screen
				{
					tempGraphUpdate = 2;
					show_page(Page::Temperature_Graph);
				}
			} break;
			case 0x55: { //enter print menu without selecting file
				tempGraphUpdate = 2;
				if (card.sdprinting == false)
				{
					constexpr const uint8 buffer[6] = {
						0x5A,
						0xA5,
						0x1D,
						0x82,
						0x01,
						0x4E
					};
					serial<2>::write(buffer);
					serial<2>::write("No SD print"_p);
				}
				show_page(Page::Print);//print menu
			} break;
					   /*case 0xFF: {
					   show_page(58); //enable lcd bridge mode
					   while (1) {
					   Tuna::wdr();
					   if (Serial.available())
					   Serial2.write(Serial.read());
					   if (Serial2.available())
					   Serial.write(Serial2.read());
					   }
					   break;
					   }*/
			default:
				break;
			}
		}

		void lcdSendMarlinVersion()
		{
			struct final
			{
				const uint8 buffer[6] = {
					0x5A,
					0xA5,
					0x12,
					0x82,
					0x05,
					0x00
				};
				const char version_str[15] = SHORT_BUILD_VERSION;
			} constexpr const version_data;

			serial<2>::write_struct(version_data);
		}

		void write_statistics()
		{
			printStatistics stats = print_job_timer.getStats();

			{
				//Total prints (including aborted)
				const uint16 totalPrints = stats.totalPrints;
				//Finished prints
				const uint16 finishedPrints = stats.finishedPrints;

				const uint8 buffer[10] = {
					0x5A, 
					0xA5, 
					0x07, //data length
					0x82, //write data to sram
					0x05,  //starting at 0x5040 vp
					0x40, 
					hi(totalPrints),
					lo(totalPrints),
					hi(finishedPrints),
					lo(finishedPrints)
				};

				serial<2>::write(buffer);
			}

			{
				struct final
				{
					const uint8 buffer[6] = {
						0x5A,
						0xA5,
						0x12,
						0x82,
						0x05,
						0x42
					};
					char time_str[15];
				} time_data;

				duration32_t{ stats.printTime }.toString<15>(time_data.time_str);

				serial<2>::write_struct(time_data);
			}

			{
				//longest print time
				struct final
				{
					const uint8 buffer[6] = {
						0x5A,
						0xA5,
						0x12,
						0x82,
						0x05,
						0x4D
					};
					char time_str[15];
				} time_data;

				duration32_t{ stats.longestPrint }.toString<15>(time_data.time_str);

				serial<2>::write_struct(time_data);
			}

			{
				//total filament used
				struct final
				{
					const uint8 buffer[6] = {
						0x5A,
						0xA5,
						0x12, //data length
						0x82, //write data to sram
						0x05, //starting at 0x0558 vp
						0x58
					};
					char filament_str[15];
				} filament_data;

				snprintf_P(filament_data.filament_str, sizeof(filament_data.filament_str), "%ld.%im"_p.c_str(), long(stats.filamentUsed / 1000.0), int(stats.filamentUsed / 100.0) % 10);

				serial<2>::write_struct(filament_data);
			}
		}
	}

	//init OK
	void initialize()
	{
		serial<2>::begin<115'200_u32>();

		lcdSendMarlinVersion();
		show_page(Page::Boot_Animation);
	}

	//lcd status update OK
	void update()
	{
		read_data();

    const auto ms = chrono::time_ms<uint16>::get();
		execute_looped_operation(ms);
		status_update(ms);
	}

	//show page OK
	void show_page(Page pageNumber)
	{
		if (pageNumber >= Page::Main_Menu) //main menu
		{
			lastPage = currentPage;
			currentPage = pageNumber;
		}
		else
		{
			lastPage = Page::Main_Menu;
			currentPage = Page::Main_Menu;
		}

		const uint8 buffer[7] = {
			0x5A,//frame header
			0xA5,
			0x04,//data length
			0x80,//command - write data to register
			0x03,
			0x00,
			uint8(pageNumber)
		};

		serial<2>::write(buffer);
	}

	void update_graph() {
		const uint16 hotend = Temperature::degHotend().rounded_to<uint16>();
		const uint16 bed = Temperature::degBed().rounded_to<uint16>();

		auto foo = type_trait<int16>::unsigned_type{ 0 };

		const uint8 buffer[9] = {
			0x5A,
			0xA5,
			0x06, //data length
			0x84, //update curve
			0x03, //channels 0,1
			hi(hotend), //TODOME
			lo(hotend), //TODOME
			hi(bed),
			lo(bed)
		};

		serial<2>::write(buffer);
	}
}
