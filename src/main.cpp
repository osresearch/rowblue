/*
 * Rowing machine BLE interface built on the ttgo-esp32.
 *
 * Quadrature decoder
 * Timestamped velocity output
 * Strokes/minute computation
 */
#include <stdint.h>
#include <stdarg.h>

#include <Arduino.h>

extern void indoorBike_setup();
extern void indoorBike_publish(float speed, float cadence, float power);
extern void cadence_publish(unsigned stroke_count, unsigned last_stroke_usec, unsigned wheel_count, unsigned last_wheel_usec);


/*
 * Quadrature modes:
 * Input Pullup, pulled low when the magnet passes.
 * Time stamp on first falling edge
 * Time stamp on second falling edge
 * No second falling edge? 
 * Reversing on the same edge?
 * Weird blip in the center of the window.
 */
#define QUAD_A 21
#define QUAD_B 22

void rower_setup()
{
	pinMode(QUAD_A, INPUT_PULLUP);
	pinMode(QUAD_B, INPUT_PULLUP);
}

int last_a;
int fall_a;
int rise_a;

int last_b;
int fall_b;
int rise_b;

unsigned long last_update;
int do_output;

unsigned start_usec;
unsigned stroke_power;
int delta_usec;
int last_delta_usec;
unsigned spm;
unsigned tick_count; // cumulative
unsigned tick_time; // start of the last stroke
unsigned ticks;
unsigned last_ticks;
float total_distance; // meters

#define PHYSICS_DT_USEC 10000
static float drift_constant = 0.2; // drop 20% speed every second
static float drag_constant = 0.002;
static unsigned long last_step;
static float oar_vel;
static float oar_force;
static float vel;
static float vel_smooth;
static float vel_smoothing = 512;
static float spm_smooth;
static float spm_smoothing = 128;

void rower_loop()
{
	// run the physics loop at least at PHYSICS_DT
	const unsigned now = micros();
	const unsigned long dt_usec = now - last_step;
	if (dt_usec > PHYSICS_DT_USEC)
	{
		// decay the boat velocity
		const float dt = dt_usec * 1.0e-6;
		last_step = now;
		vel -= vel * drift_constant * dt;

		// add any force from the oar, minus the drag of the
		const float drag = vel * vel * drag_constant;
		vel += (oar_force - drag) * dt;

		// low-pass filter the velocity for output
		vel_smooth = (vel_smooth * vel_smoothing + vel) / (vel_smoothing + 1);
		spm_smooth = (spm_smooth * spm_smoothing + spm) / (spm_smoothing + 1);

		// integrate for total distance
		total_distance += vel_smooth * dt;
	}

	int got_tick = 0;
	const int a = digitalRead(QUAD_A);
	const int b = digitalRead(QUAD_B);

	if (a && b)
	{
		// back to idle, so prep for the next tick output
		do_output = 1;
	}

	if (!a && last_a)
	{
		if (now - rise_a > 20000)
			fall_a = now;

		if (!b && do_output)
		{
			// b is already low, so this is a negative speed
			do_output = 0;
			got_tick = 1;
			delta_usec = fall_b - now;
		}
	}
			
	if (a && !last_a)
	{
		rise_a = now;
	}

	if (!b && last_b)
	{
		if (now - rise_b > 20000)
			fall_b = now;

		if (!a && do_output)
		{
			// a is already low, so this is a positive speed
			do_output = 0;
			got_tick = 1;
			delta_usec = now - fall_a;
		}
	}

	if (b && !last_b)
	{
		rise_b = now;
	}

	last_a = a;
	last_b = b;

	if (!got_tick)
	{
		if (now - last_update < 1000000)
			return;
		// it has been a second since our update, send a zero-force notice
		delta_usec = 0;
		spm = 0;
		stroke_power = 0;
	}

	// on positive velocity pulls, check for a sign change
	// for tracking power and distance
	if (delta_usec > 0)
	{
		// if the delta_usec is too fast, this is probably an error
		// and we should discard this point.100000000
		if (delta_usec < 2500)
			return;

		// need a scaling factor to compute how much
		// force they are applying.
		oar_vel = 1.0e5 / delta_usec;
		oar_force = oar_vel * oar_vel;

		if (last_delta_usec <= 0)
		{
			// sign change: this is the start of a new stroke
			// compute spm * 10
			const unsigned stroke_delta = now - start_usec;
			spm = 600000000L / stroke_delta;
			
			start_usec = now;
			stroke_power = 0;
			last_ticks = ticks;
			ticks = 0;

			// blank line to mark the log
			Serial.println();
		}


		stroke_power += oar_force;
		ticks++;
	} else {
		// return stroke increment stroke counter on first call
		if (oar_vel > 0)
		{
			tick_time = now;
			tick_count++;
		}
		oar_vel = 0;
		oar_force = 0;
	}

	last_delta_usec = delta_usec;
	last_update = now;

	String msg = String("")
		+ now + ","				// 0
		+ (now - start_usec) + ","		// 1
		+ delta_usec + ","			// 2
		+ String(oar_force,3) + ","		// 3
		+ stroke_power + ","			// 4
		+ String(spm_smooth/10,1)		// 5
		+ "," + String(vel,3)			// 6
		+ "," + String(vel_smooth,1)		// 7
		+ "," + String(total_distance,1)	// 8
		;

	const int debug_serial = 0;
	if (debug_serial)
		Serial.println(msg);
}


void setup(void)
{
	Serial.begin(115200);
	indoorBike_setup();
	rower_setup();
}

// GATT specification (GSS) has the fitness device details
void send_update(void)
{
	//indoorBike_publish(spm_smooth/10, vel_smooth, stroke_power);

	// convert our distance estimate into wheel rotations
	// 700x25c, according to the garmin table
	float wheel_circumference = 2.105; // 0.7 * 2 * M_PI;
	unsigned wheel_count = total_distance / wheel_circumference / 3; // because too fast

	String msg = String(spm_smooth,1) + "," + String(tick_count,1) + "," + String(vel_smooth,1) + "," + String((float)stroke_power,0) + "," + String(total_distance,1);
	Serial.println(msg);
	//cadence_publish(last_crank_count, last_crank_time, wheel_count, micros());
	cadence_publish(tick_count*3, tick_time, wheel_count, micros());
}

void loop(void)
{
	rower_loop();

	const unsigned now = micros();
	static unsigned last_update = 0;
	unsigned update_usec = 250000; // 4 Hz

	// only send every few seconds if nothing is happening
	if (vel_smooth < 0.5)
		update_usec = 5000000; // 1/5 Hz

	if (now - last_update > update_usec)
	{
		last_update = now;
		send_update();
	}
		
}

void fake_loop(void)
{
	const unsigned now = micros();
	static unsigned last_update = 0;
	unsigned update_usec = 500000; // 2 Hz

	static unsigned tick_count = 0;
	static unsigned wheel_count = 0;

	if (now - last_update > update_usec)
	{
		last_update = now;
		tick_count++;
		tick_time = now;
		wheel_count += 10;
		cadence_publish(tick_count, now, wheel_count, now);
		Serial.println(tick_count);
	}
}
