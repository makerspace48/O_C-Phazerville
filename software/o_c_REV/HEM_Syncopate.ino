// Copyright (c) 2024, Nicholas Michalek
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "HSProbLoopLinker.h"

class Syncopate : public HemisphereApplet {
public:

    static constexpr uint8_t MAX_DIV = 63;

    enum SyncopateCursor {
        DIV1, DIV2, DIV3, DIV4,
        TOGGLE_A1, TOGGLE_A2, TOGGLE_A3, TOGGLE_A4,
        TOGGLE_B1, TOGGLE_B2, TOGGLE_B3, TOGGLE_B4,
        LAST_SETTING = TOGGLE_B4
    };

    struct ClkDivider {
      uint8_t steps;
      uint8_t clock_count;

      void Set(int s) {
        steps = constrain(s, 0, MAX_DIV);
      }
      bool Poke() {
        if (steps == 0) return false;
        if (++clock_count > steps) {
            clock_count = 1;
        }
        return (clock_count == 1);
      }
      void Reset() {
        clock_count = 0;
      }

    } divider[4] = {
        { 4, 0 }, { 3, 0 }, { 0, 0 }, { 0, 0 }
    };

    const char* applet_name() {
        return "Syncopate";
    }

    void Start() {
    }

    void Reset() {
        for (int ch = 0; ch < 4; ++ch) {
          divider[ch].Reset();
        }
    }
    void TrigOut(int ch) {
        ClockOut(ch);
        loop_linker->Trigger(ch);
    }
    bool Enabled(int ch, int div_id) {
        return (div_enabled >> (ch*4 + div_id)) & 0x01;
    }
    void ToggleDiv(int idx) {
        div_enabled ^= (0x01 << idx);
    }
    
    void Controller() {
        static int currentStep = 0; // Tracks the current step in the sequence
        const int totalSteps = 4;   // Total number of steps in the sequence
    
        loop_linker->RegisterDiv(hemisphere);
    
        // reset
        if (Clock(1)) {
            Reset();
            currentStep = 0; // Reset the sequence step
        }
    
        if (Clock(0)) {
            bool advanceStep = false;
    
            // Process the current step
            if (divider[currentStep].steps == 0) {
                // If current divider is set to zero, skip this step
                advanceStep = true;
            } else if (divider[currentStep].Poke()) {
                // If current step is completed, advance to next step
                advanceStep = true;
            }
    
            if (advanceStep) {
                currentStep = (currentStep + 1) % totalSteps;
            }
    
            // Determine the trigger status for each step
            bool trig_q[4] = {false, false, false, false};
            trig_q[currentStep] = advanceStep;
    
            ForAllChannels(ch) {
                bool trig = false;
                bool xor_mode = (In(ch) > 6*128);
    
                ForAllChannels(i) {
                    if (Enabled(ch, i)) {
                        trig = xor_mode ? (trig != trig_q[i]) : (trig || trig_q[i]);
                    }
    
                    if (trig_q[i]) pulse_animation[i] = HEMISPHERE_PULSE_ANIMATION_TIME;
                }
    
                if (trig) TrigOut(ch);
            }
        }
    
        ForAllChannels(ch) {
            if (pulse_animation[ch] > 0) {
                pulse_animation[ch]--;
            }
        }
    }



    void View() {
        DrawInterface();
    }

    void OnButtonPress() {
        if (cursor >= TOGGLE_A1 && !EditMode())
            ToggleDiv(cursor - TOGGLE_A1);
        else
            CursorAction(cursor, LAST_SETTING);
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, LAST_SETTING);
            return;
        }

        if (cursor <= DIV4) {
            divider[cursor].Set(divider[cursor].steps + direction);
        } else
            ToggleDiv(cursor - TOGGLE_A1);
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        const size_t b = 6; // bitsize

        Pack(data, PackLocation {0, 8}, div_enabled);
        ForAllChannels(i) {
            Pack(data, PackLocation{8 + i*b, b}, divider[i].steps);
        }
        return data;
    }

    void OnDataReceive(uint64_t data) {
        const size_t b = 6; // bitsize
        div_enabled = Unpack(data, PackLocation {0, 8});
        ForAllChannels(i) {
            divider[i].Set( Unpack(data, PackLocation{8 + i*b, b}) );
        }
        Reset();
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock  2=Reset";
        help[HEMISPHERE_HELP_CVS]      = "1=XOR    2=XOR";
        help[HEMISPHERE_HELP_OUTS]     = "A=Trig   B=Trig";
        help[HEMISPHERE_HELP_ENCODER]  = "Dividers/Toggles";
        //                               "------------------" <-- Size Guide
    }
    
private:
    int cursor; // SyncopateCursor 

    int pulse_animation[4] = {0,0,0,0};
    uint8_t div_enabled; // bitmask for enabling dividers per output
                         // bits 0-3 for A, bits 4-7 for B

    ProbLoopLinker *loop_linker = loop_linker->get();

    void DrawInterface() {
      gfxPrint(32, 14, "A B");
      ForAllChannels(ch) {
        const int y = 24 + ch*10;
        gfxPrint(1, y, "/");
        gfxPrint(divider[ch].steps);
        if (pulse_animation[ch]) gfxInvert(1, y, 19, 9);

        gfxIcon(32, y, Enabled(0, ch) ? CHECK_ON_ICON : CHECK_OFF_ICON);
        gfxIcon(44, y, Enabled(1, ch) ? CHECK_ON_ICON : CHECK_OFF_ICON);
      }

      if (cursor <= DIV4)
        gfxCursor(7, 32 + cursor*10, 13);
      else {
        const int x = 31 + 12*((cursor - TOGGLE_A1) / 4);
        const int y = 23 + 10*((cursor - TOGGLE_A1) % 4);
        gfxFrame(x, y, 10, 10);
      }
    }

};


////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Applet Functions
///
///  Once you run the find-and-replace to make these refer to Syncopate,
///  it's usually not necessary to do anything with these functions. You
///  should prefer to handle things in the HemisphereApplet child class
///  above.
////////////////////////////////////////////////////////////////////////////////
Syncopate Syncopate_instance[2];

void Syncopate_Start(bool hemisphere) {Syncopate_instance[hemisphere].BaseStart(hemisphere);}
void Syncopate_Controller(bool hemisphere, bool forwarding) {Syncopate_instance[hemisphere].BaseController(forwarding);}
void Syncopate_View(bool hemisphere) {Syncopate_instance[hemisphere].BaseView();}
void Syncopate_OnButtonPress(bool hemisphere) {Syncopate_instance[hemisphere].OnButtonPress();}
void Syncopate_OnEncoderMove(bool hemisphere, int direction) {Syncopate_instance[hemisphere].OnEncoderMove(direction);}
void Syncopate_ToggleHelpScreen(bool hemisphere) {Syncopate_instance[hemisphere].HelpScreen();}
uint64_t Syncopate_OnDataRequest(bool hemisphere) {return Syncopate_instance[hemisphere].OnDataRequest();}
void Syncopate_OnDataReceive(bool hemisphere, uint64_t data) {Syncopate_instance[hemisphere].OnDataReceive(data);}
