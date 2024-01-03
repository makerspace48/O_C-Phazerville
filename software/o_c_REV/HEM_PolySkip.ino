// Copyright (c) 2022, Benjamin Rosenbach
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

#include "HSProbLoopLinker.h" // singleton for linking ProbDiv and ProbMelo

class PolySkip : public HemisphereApplet {
public:

    static constexpr int MAX_DIV = 32;

    enum PolySkipCursor {
        STEP1, STEP2, STEP3, STEP4, STEP5,
        NUM_STEPS,
        LAST_SETTING = STEP5
    };

    const char* applet_name() {
        return "PolySkip";
    }

    void Start() {
    }

    void Reset() {
        step_index = -1;
        clock_count = 0;
        reset_animation = HEMISPHERE_PULSE_ANIMATION_TIME_LONG;
    }
    void TrigOut() {
        ClockOut(0);
        loop_linker->Trigger(0);
        pulse_animation = HEMISPHERE_PULSE_ANIMATION_TIME;
    }

    void Controller() {
        loop_linker->RegisterDiv(hemisphere);

        // reset
        if (Clock(1)) {
            Reset();
        }

        if (Clock(0)) {
            // reset case
            if (step_index < 0) {
                ++step_index;
                TrigOut();
                return;
            }

            // quota achieved, trig out and advance
            if (++clock_count >= steps[step_index]) {
                TrigOut();
                clock_count = 0;
                do { // lol this will get stuck if anything goes wrong...
                    ++step_index %= NUM_STEPS;
                } while (steps[step_index] == 0);
                return;
            }

            ClockOut(1); // skipped clocks
            loop_linker->Trigger(1);
        }

        if (pulse_animation > 0) {
            pulse_animation--;
        }
        if (reset_animation > 0) {
            reset_animation--;
        }
    }

    void View() {
        DrawInterface();
    }

    void OnButtonPress() {
        CursorAction(cursor, LAST_SETTING);
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, LAST_SETTING);
            return;
        }

        steps[cursor] = constrain(steps[cursor] + direction, cursor == STEP1 ? 1 : 0, MAX_DIV);
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        for (size_t i = 0; i < NUM_STEPS; i++) {
            Pack(data, PackLocation {0 + i*5, 5}, steps[i]);
        }
        return data;
    }

    void OnDataReceive(uint64_t data) {
        for (size_t i = 0; i < NUM_STEPS; i++) {
            steps[i] = Unpack(data, PackLocation {0 + i*5, 5});
            steps[i] = constrain(steps[i], 0, MAX_DIV);
        }
        // step 1 cannot be zero
        if (steps[0] == 0) ++steps[0];
        Reset();
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock  2=Reset";
        help[HEMISPHERE_HELP_CVS]      = "1=       2=";
        help[HEMISPHERE_HELP_OUTS]     = "A=Trig   B=Skips";
        help[HEMISPHERE_HELP_ENCODER]  = "Weights/Loop";
        //                               "------------------" <-- Size Guide
    }
    
private:
    int cursor; // PolySkipCursor 
    int step_index;
    int clock_count;
    uint8_t steps[NUM_STEPS] = { 1, 2, 0, 0, 0 }; // duration as number of clock pulses

    int pulse_animation = 0;
    int reset_animation = 0;

    ProbLoopLinker *loop_linker = loop_linker->get();

    void DrawInterface() {
        // divisions
        for(int i = 0; i < NUM_STEPS; i++) {
          gfxPrint(1, 15 + (i*10), "/");
          gfxPrint(steps[i]);
          DrawKnobAt(21, 15 + (i*10), 33, steps[i], cursor == i);
          if (step_index == i)
            gfxIcon(55, 15 + i*10, LEFT_BTN_ICON);

        }
        // flash division when triggered
        if (pulse_animation > 0 && step_index >= 0) {
          gfxInvert(1, 15 + (step_index*10), 12, 8);
        }
    }

    void DrawKnobAt(uint8_t x, uint8_t y, uint8_t len, uint8_t value, bool is_cursor) {
        uint8_t p = is_cursor ? 1 : 3;
        uint8_t w = Proportion(value, MAX_DIV, len-1);
        gfxDottedLine(x, y + 3, x + len, y + 3, p);
        gfxRect(x + w, y, 2, 7);
        if (EditMode() && is_cursor) gfxInvert(x-1, y, len+3, 7);
    }

};


////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Applet Functions
///
///  Once you run the find-and-replace to make these refer to PolySkip,
///  it's usually not necessary to do anything with these functions. You
///  should prefer to handle things in the HemisphereApplet child class
///  above.
////////////////////////////////////////////////////////////////////////////////
PolySkip PolySkip_instance[2];

void PolySkip_Start(bool hemisphere) {PolySkip_instance[hemisphere].BaseStart(hemisphere);}
void PolySkip_Controller(bool hemisphere, bool forwarding) {PolySkip_instance[hemisphere].BaseController(forwarding);}
void PolySkip_View(bool hemisphere) {PolySkip_instance[hemisphere].BaseView();}
void PolySkip_OnButtonPress(bool hemisphere) {PolySkip_instance[hemisphere].OnButtonPress();}
void PolySkip_OnEncoderMove(bool hemisphere, int direction) {PolySkip_instance[hemisphere].OnEncoderMove(direction);}
void PolySkip_ToggleHelpScreen(bool hemisphere) {PolySkip_instance[hemisphere].HelpScreen();}
uint64_t PolySkip_OnDataRequest(bool hemisphere) {return PolySkip_instance[hemisphere].OnDataRequest();}
void PolySkip_OnDataReceive(bool hemisphere, uint64_t data) {PolySkip_instance[hemisphere].OnDataReceive(data);}
