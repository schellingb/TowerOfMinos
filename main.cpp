/*
  Tower of Minos
  Copyright (C) 2019 Bernhard Schelling

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include <ZL_Application.h>
#include <ZL_Display.h>
#include <ZL_Surface.h>
#include <ZL_Signal.h>
#include <ZL_Audio.h>
#include <ZL_Font.h>
#include <ZL_Scene.h>
#include <ZL_Input.h>
#include <ZL_SynthImc.h>
#include <vector>

enum
{
	VIEW_HEIGHT = 20,
	VIEW_HALF = 10,
	WELL_WIDTH = 10,
	WELL_HALF = 5,
};
#define PLAYER_WIDTH .3f
#define PLAYER_HEIGHT .45f
#define PLAYER_SCALE .03f
#define TOMTPF (1.f/60.f)
#define TOMELAPSEDF(factor) (TOMTPF*(s(factor)))

/*
static ZL_Color falling_colors[] =
{
	ZLRGBFF(255, 255, 255),
	ZLRGBFF(  0, 240, 240),
	ZLRGBFF(  0,   0, 240),
	ZLRGBFF(240,   0, 160),
	ZLRGBFF(240, 240,   0),
	ZLRGBFF(  0, 240,   0),
	ZLRGBFF(160,   0, 240),
	ZLRGBFF(240,   0,   0),
};
static ZL_Color landed_colors[] =
{
	ZLRGBFF(255, 255, 255),
	ZLRGBFF(  0, 180, 180),
	ZLRGBFF(  0,   0, 180),
	ZLRGBFF(180,   0, 100),
	ZLRGBFF(180, 180,   0),
	ZLRGBFF(  0, 180,   0),
	ZLRGBFF(100,   0, 180),
	ZLRGBFF(180,   0,   0),
};
*/

static ZL_Color landed_colors[] =
{
	ZLRGBFF(255, 255, 255),
	ZLRGBFF( 43, 178, 233),
	ZLRGBFF( 39,  72, 141),
	ZLRGBFF(161,  46, 137),
	ZLRGBFF(205, 127,  44),
	ZLRGBFF( 83, 159,  68),
	ZLRGBFF(235, 219,  97),
	ZLRGBFF(186,  40,  59),
};
static ZL_Color falling_colors[] =
{
	landed_colors[0]*1.2f,
	landed_colors[1]*1.2f,
	landed_colors[2]*1.2f,
	landed_colors[3]*1.2f,
	landed_colors[4]*1.2f,
	landed_colors[5]*1.2f,
	landed_colors[6]*1.2f,
	landed_colors[7]*1.2f,
};

struct Block
{
	int x, prevy;
	float y;
	int shape, color;
	Block(int x, float y, int shape, int color) : x(x), y(y), prevy((int)y), shape(shape), color(color) {}
};

struct Player
{
	float x, y;
	float velx, vely;
	bool dead, stand_landed, stand_falling;
	int jump, jumps;
	ticks_t standTicks;
};

static bool titleScreen = true;
static std::vector<Block> falling, landed;
static Player player;
static int score_y;
static float scroll_y = VIEW_HALF;
static float fall_vel;
static ZL_Surface srfBG, srfBlocks, srfPlayer, srfStripes, srfLudumDare;
static int well_tops[WELL_WIDTH];
static ticks_t failTicks;
static ZL_Font fntMain;
static ZL_TextBuffer txtGameOver, txtTitle;
static ZL_TextBuffer txt[6];
static ticks_t startTicks;
static ticks_t upgradeTicks;
static ticks_t deadTicks;
static float shake = 0;

extern ZL_SynthImcTrack imcJump;
extern ZL_SynthImcTrack imcDeath;
extern ZL_SynthImcTrack imcFall;
extern ZL_SynthImcTrack imcLand;
extern ZL_SynthImcTrack imcLvlUp;
extern ZL_SynthImcTrack imcMusic;

static void Init()
{
	score_y = 0;
	scroll_y = VIEW_HALF;
	falling.clear();
	landed.clear();
	for (int i = 0; i != WELL_WIDTH; i++)
	{
		landed.push_back(Block(i, 0, 0, 0));
		well_tops[i] = 1;
	}
	failTicks = 0;
	startTicks = ZLTICKS;
	upgradeTicks = ZLTICKS;
	deadTicks = 0;
	shake = 0;

	player.x = WELL_HALF;
	player.y = 1;
	player.velx = player.vely = 0;
	player.dead = false;
	player.stand_landed = true;
	player.stand_falling = false;
	player.standTicks = ZLTICKS;
	player.jump = 0;
	player.jumps = 1;
}

static void SpawnBlock()
{
	fall_vel = 0;
	int level = 4 + score_y / 10;
	int max_height = 2 * player.jumps;
	int shape = RAND_INT_RANGE(0,3), color = RAND_INT_RANGE(1,COUNT_OF(falling_colors)-1);
	for (int retry_shape = 0; retry_shape < 10; retry_shape++)
	{
		int num = RAND_INT_RANGE(1, level);
		falling.push_back(Block(0, 0, shape, color));
		ZL_Rect rec(0, 1, 1, 0);
		for (int x = 0, y = 0, i = 1; i < num; i++)
		{
			int dir = RAND_INT_RANGE(0, 3);
			if (rec.Height() >= max_height && (dir == 1 || dir == 3)) { i--; continue; }
			x += (dir == 0 ? 1 : (dir == 2 ? -1 : 0));
			y += (dir == 1 ? 1 : (dir == 3 ? -1 : 0));

			bool already_blocked = false;
			for (Block& b : falling) { if (b.x == x && b.y == y) { already_blocked = true; break; } }
			if (already_blocked) { i--; continue; }

			falling.push_back(Block(x, (float)y, shape, color));

			if (x   < rec.left  ) rec.left   = x;
			if (x+1 > rec.right ) rec.right  = x+1;
			if (y   < rec.bottom) rec.bottom = y;
			if (y+1 > rec.top   ) rec.top    = y+1;
		}
		if ((rec.right - rec.left) > (WELL_WIDTH-4))
		{
			falling.clear();
			retry_shape--;
			continue;
		}
		int max_y = score_y + 1 + (2 * (player.jumps - 1)) - (rec.top - rec.bottom);
		if (max_y < 0) max_y = 0;
		int spawn_start = -rec.left, spawn_width = WELL_WIDTH-(rec.right - rec.left)+1;
		int rand_x = RAND_INT_RANGE(0, spawn_width - 1);
		int spawn_x;
		bool valid;
		for (int retry = 0; retry < WELL_WIDTH; retry++)
		{
			spawn_x = spawn_start + ((rand_x + retry) % spawn_width);
			valid = true;
			for (int i = rec.left; valid && i != rec.right; i++)
			{
				int setInvalid = (well_tops[spawn_x + i] > max_y);
				if (setInvalid)
				{
					valid = false;
				}
			}
			if (valid) break;
		}
		if (!valid)
		{
			falling.clear();
			continue;
		}
		for (Block& b : falling)
		{
			b.x += spawn_x;
			b.y += scroll_y + VIEW_HALF + (rec.top - rec.bottom);
			b.prevy = (int)b.y;
		}
		failTicks = 0;
		imcFall.Play(true);
		return;
	}
	if (!failTicks)
		failTicks = ZLTICKS;
}

static void CheckCollision(bool check_y)
{
	ZL_Vector player_pos(player.x+PLAYER_WIDTH, player.y+PLAYER_HEIGHT);
	ZL_Rectf player_rec(player_pos, ZLV(PLAYER_WIDTH, PLAYER_HEIGHT));
	const float collision_check_dist = (PLAYER_HEIGHT + .5f + .2f);
	const float collision_check_radsq = collision_check_dist*collision_check_dist*2;
	for (int i = 0; i != 2; i++)
	{
		const float vely_vs_block = (player.vely - (i ? fall_vel : 0));
		for (Block& l : (i ? falling : landed))
		{
			ZL_Vector block_pos(l.x+.5f, l.y+.5f);
			if (player_pos.GetDistanceSq(block_pos) > collision_check_radsq) continue;
			ZL_Rectf block_rec(block_pos, .5f);
			if (check_y)
			{
				if (vely_vs_block <= .1f && player_rec.low - .05f < block_rec.high && player_rec.high > block_rec.high && player_rec.left+.01f < block_rec.right && player_rec.right-.01f > block_rec.left)
				{
					player.vely = 0;
					player.jump = 0;
					player.y = block_rec.high;
					(i ? player.stand_falling : player.stand_landed) = true;
					player.standTicks = ZLTICKS;
					player_pos = ZL_Vector(player.x+PLAYER_WIDTH, player.y+PLAYER_HEIGHT);
					player_rec = ZL_Rectf(player_pos, ZLV(PLAYER_WIDTH, PLAYER_HEIGHT));
				}
				else if (player.stand_landed && player_rec.left > block_rec.left-.1f && player_rec.right < block_rec.right+.1f && player_rec.low > block_rec.low-.1f && player_rec.high < block_rec.high+.1f)
				{
					player.dead = true;
					imcDeath.Play(true);
					deadTicks = ZLTICKS;
				}
				if (vely_vs_block >= -.1f && player_rec.high + .05f > block_rec.low && player_rec.low < block_rec.low && player_rec.left+.01f < block_rec.right && player_rec.right-.01f > block_rec.left)
				{
					if (player.vely > 0) player.vely = 0;
					player.y = block_rec.low - (PLAYER_HEIGHT*2);
					player_pos = ZL_Vector(player.x+PLAYER_WIDTH, player.y+PLAYER_HEIGHT);
					player_rec = ZL_Rectf(player_pos, ZLV(PLAYER_WIDTH, PLAYER_HEIGHT));
				}
			}
			//if (check_x)
			{
				if (player_rec.right > block_rec.left && player_rec.left < block_rec.left && player_rec.low+.01f < block_rec.high && player_rec.high-.01f > block_rec.low)
				{
					player.velx = 0;
					player.x = block_rec.left - (PLAYER_WIDTH*2);
					player_pos = ZL_Vector(player.x+PLAYER_WIDTH, player.y+PLAYER_HEIGHT);
					player_rec = ZL_Rectf(player_pos, ZLV(PLAYER_WIDTH, PLAYER_HEIGHT));
				}
				if (player_rec.left < block_rec.right && player_rec.right > block_rec.right && player_rec.low+.01f < block_rec.high && player_rec.high-.01f > block_rec.low)
				{
					player.velx = 0;
					player.x = block_rec.right;
					player_pos = ZL_Vector(player.x+PLAYER_WIDTH, player.y+PLAYER_HEIGHT);
					player_rec = ZL_Rectf(player_pos, ZLV(PLAYER_WIDTH, PLAYER_HEIGHT));
				}
			}
		}
	}
	if (player.x < 0)
	{
		player.x = 0;
	}
	if (player.x > WELL_WIDTH - (PLAYER_WIDTH*2))
	{
		player.x = WELL_WIDTH - (PLAYER_WIDTH*2);
	}
}

static void Update()
{
	if (titleScreen)
		return;

	if (ZLSINCE(startTicks) < 500)
	{
		return;
	}
	if (ZL_Input::Down(ZLK_ESCAPE, true))
	{
		Init();
		titleScreen = true;
		imcMusic.SetSongVolume(40);
		return;
	}

	if (player.dead)
	{
		if (ZLSINCE(deadTicks) > 500 && ZL_Input::Down(ZLK_SPACE, true))
		{
			Init();
		}
		else
		{
			return;
		}
	}

	player.velx = 
		(ZL_Input::Held(ZLK_A) || ZL_Input::Held(ZLK_LEFT) ? -1.f : 0.f) +
		(ZL_Input::Held(ZLK_D) || ZL_Input::Held(ZLK_RIGHT) ? 1.f : 0.f);

	if (ZL_Input::Down(ZLK_SPACE, true) && (player.stand_landed || player.stand_falling || ZLSINCE(player.standTicks) < 120 || (player.jump > 0 && player.jump < player.jumps)))
	{
		player.stand_landed = player.stand_falling = false;
		player.vely = 3;
		player.jump++;
		imcJump.Play(true);
	}

	fall_vel -= TOMELAPSEDF(6);
	float current_fall_vel = fall_vel * TOMELAPSEDF(3);

	if (falling.size() == 0)
	{
		SpawnBlock();
	}

#ifdef ZILLALOG
	if (ZL_Input::Down(ZLK_L))
	{
		player.y = scroll_y + 3;
		fall_vel = -.5;
		falling.clear();
		for (int i = 0; i != WELL_WIDTH; i++)
			falling.push_back(Block(i, scroll_y, 0, 0));
	}
#endif

	int collide_height = 0;
	for (Block& b : falling)
	{
		b.y += current_fall_vel;
		int iy = (int)b.y;
		if (iy < b.prevy)
		{
			for (int y = b.prevy - 1; y >= iy; y--)
				for (Block& l : landed)
					if (l.x == b.x && l.prevy == y)
						{ 
							collide_height = MAX(collide_height, y - iy + 1);
							break; }
			b.prevy = iy;
		}
	}
	if (collide_height)
	{
		for (Block& b : falling)
		{
			b.y = (float)(b.prevy += collide_height);
			well_tops[b.x] = b.prevy;
			landed.push_back(b);
		}
		falling.clear();
		imcLand.Play(true);
		shake = .5f;
	}

	if (player.stand_falling)
	{
		player.vely = fall_vel;
		player.y += current_fall_vel;
	}
	if (player.velx)
	{
		player.x += player.velx * TOMELAPSEDF(6);
		CheckCollision(false);
	}
	if (!player.stand_landed && !player.stand_falling)
	{
		player.vely -= TOMELAPSEDF(8);
		player.y += player.vely * TOMELAPSEDF(4);
	}

	player.stand_landed = player.stand_falling = false;
	CheckCollision(true);

	if (player.y > scroll_y)
		scroll_y = player.y;

	if (player.stand_landed && (int)player.y > score_y)
	{
		score_y = (int)player.y;
		
		txt[1] = fntMain.CreateBuffer(ZL_String(score_y));
		if (score_y < 10)
		{
			txt[3] = fntMain.CreateBuffer("Single Jump");
			txt[4] = fntMain.CreateBuffer("Get Double Jump at:");
			txt[5] = fntMain.CreateBuffer("10");
			player.jumps = 1;
		}
		else if (score_y >= 10 && score_y < 30)
		{
			txt[3] = fntMain.CreateBuffer("Double Jump");
			txt[4] = fntMain.CreateBuffer("Get Tripple Jump at:");
			txt[5] = fntMain.CreateBuffer("30");
			if (player.jumps != 2)
			{
				imcLvlUp.Play(true);
				upgradeTicks = ZLTICKS;
				player.jumps = 2;
			}
		}
		else
		{
			txt[3] = fntMain.CreateBuffer("Tripple Jump");
			txt[4] = fntMain.CreateBuffer("");
			txt[5] = fntMain.CreateBuffer("");
			if (player.jumps != 3)
			{
				imcLvlUp.Play(true);
				upgradeTicks = ZLTICKS;
				player.jumps = 3;
			}
		}
	}

	if (player.y < scroll_y - VIEW_HALF - .5f)
	{
		player.dead = true;
		imcDeath.Play(true);
		deadTicks = ZLTICKS;
	}

	if (failTicks && ZLSINCE(failTicks) > 1000)
	{
		player.dead = true;
		imcDeath.Play(true);
		deadTicks = ZLTICKS;
	}
}

static void DrawTextBordered(const ZL_Vector& p, const char* txt, scalar scale = 1, const ZL_Color& colfill = ZLWHITE, const ZL_Color& colborder = ZLBLACK, int border = 2, ZL_Origin::Type origin = ZL_Origin::Center)
{
	for (int i = 0; i < 9; i++) if (i != 4) fntMain.Draw(p.x+(border*((i%3)-1)), p.y+8+(border*((i/3)-1)), txt, scale, scale, colborder, origin);
	fntMain.Draw(p.x  , p.y+8  , txt, scale, scale, colfill, origin);
}

static void Draw()
{
	static const ZL_Color colOutGradientTop    = ZLRGB( 0, 0,.4);
	static const ZL_Color colOutGradientBottom = ZLRGB(.4,.4,.8);
	static const ZL_Color colInGradientTop     = ZLRGB( 0, 0,.2);
	static const ZL_Color colInGradientBottom  = ZLRGB(.2,.2,.4);
	static const ZL_Color colStripes           = ZLRGB(.5,.7,.9);
	static const ZL_Color colShadow            = ZLLUMA(0, .6);

	ZL_Rectf view(WELL_HALF, scroll_y, ZLV(VIEW_HALF*ZLASPECTR, VIEW_HALF));
	ZL_Display::PushOrtho(view);

	if (titleScreen || ZLSINCE(startTicks) < 500)
	{
		float t = (titleScreen ? 0 : ZL_Easing::InQuad(ZLSINCE(startTicks) / 500.f));
		ZL_Display::Translate(view.Center());
		ZL_Display::Scale(10.f - 9.f * t);
		ZL_Display::Translate(-view.Center());
	}

	if (shake > .1f)
	{
		shake *= .9f;
		ZL_Display::Translate(RAND_ANGLEVEC*shake);
	}

	ZL_Display::ClearFill(colOutGradientTop);
	ZL_Display::FillRect(0, view.low - 1, WELL_WIDTH, view.high + 1, colInGradientTop);
	ZL_Display::FillGradient(0, -1, WELL_WIDTH, 100, colInGradientTop, colInGradientTop, colInGradientBottom, colInGradientBottom);
	srfBG.DrawTo(0.f, (float)(int)view.low-1, (float)WELL_WIDTH, view.high+1, ZLRGBA(.1,.1,.25,.5));

	if (titleScreen)
	{
		if (ZL_Input::Down(ZLK_ESCAPE, true))
		{
			ZL_Application::Quit();
		}
		if (ZL_Input::Down(ZLK_SPACE, true))
		{
			titleScreen = false;
			imcMusic.SetSongVolume(20);
			Init();
		}
		ZL_Display::PopOrtho();

		ZL_Vector titlePos = ZLV(ZLHALFW, ZLHALFH+130);
		ZL_Color colOuter = ZLLUMA(0, .1), colInner = ZLHSVA(smod(ZLTICKS*.001f,1.f),1,1, .2);
		float t = ZL_Easing::InQuad(ssin(ZLTICKS*.001f)*.5f+.5f)*.25f;
		for (float scale = 10; scale >= 0; scale--)
			for (int i = 0; i != 9; i++)
				txtTitle.Draw(titlePos+ZLV(i/3-1,(i%3)-1)*3, 4-scale*t, 4-scale*t, colOuter, ZL_Origin::Center);
		for (float scale = 10; scale >= 0; scale--)
			txtTitle.Draw(titlePos, 4-scale*t, 4-scale*t, colInner, ZL_Origin::Center);

		ZL_Color ColText = ZLRGBA(.6,.8,1,.75), ColBorder = ZLLUMA(0,.5);
		DrawTextBordered(ZLV(ZLHALFW,210), "Climb the Tower of Minos without getting crushed!", 1.f, ColText, ColBorder);
		DrawTextBordered(ZLV(ZLHALFW,150), "'A' Move left        'D' Move right        'SPACE' Jump", 1.f, ColText, ColBorder);
		DrawTextBordered(ZLV(ZLHALFW,100), "PRESS 'SPACE' TO BEGIN", 0.8f, ColText, ColBorder);
		DrawTextBordered(ZLV(ZLHALFW, 50), "'ALT-ENTER' Toggle Fullscreen", 0.5f, ColText, ColBorder);
		DrawTextBordered(ZLV(18, 12), "2019 - Bernhard Schelling", s(.6), ZLRGBA(.5,.7,.8,.5), ColBorder, 2, ZL_Origin::BottomLeft);

		srfLudumDare.Draw(ZLFROMW(10), 10);

		return;
	}

	srfBlocks.BatchRenderBegin(true);
	float shadowx = .2f, shadowy = .2f - (MIN(scroll_y, 100.f) / 333.f);
	for (Block& b : landed)
	{
		if (b.y - 1 > view.high || b.y + 2 < view.low) continue;
		srfBlocks.DrawTo((float)b.x+shadowx, (float)b.y+shadowy, (float)b.x+1+shadowx, (float)b.y+1+shadowy, colShadow);
	}
	for (Block& b : falling)
	{
		if (b.y - 1 > view.high || b.y + 2 < view.low) continue;
		srfBlocks.DrawTo((float)b.x+shadowx, (float)b.y+shadowy, (float)b.x+1+shadowx, (float)b.y+1+shadowy, colShadow);
	}

	for (Block& b : landed)
	{
		if (b.y - 1 > view.high || b.y + 2 < view.low) continue;
		//ZL_Display::FillRect(b.x, b.y, b.x+1, b.y+1, ZL_Color::Yellow);
		srfBlocks.SetTilesetIndex(b.shape).DrawTo((float)b.x, (float)b.y, (float)b.x+1, (float)b.y+1, landed_colors[b.color]);
	}
	for (Block& b : falling)
	{
		if (b.y - 1 > view.high || b.y + 2 < view.low) continue;
		//ZL_Display::FillRect(b.x, b.y, b.x+1, b.y+1, ZL_Color::Red);
		srfBlocks.SetTilesetIndex(b.shape).DrawTo((float)b.x, (float)b.y, (float)b.x+1, (float)b.y+1, falling_colors[b.color]);
	}
	srfBlocks.BatchRenderEnd();

	if (!player.dead)
	{
		//ZL_Display::FillRect(player.x, player.y, player.x+PLAYER_WIDTH*2, player.y+PLAYER_HEIGHT*2, ZL_Color::Pink);
		//ZL_Display::DrawCircle(player.x+.4f, player.y+.4f, .4f, ZL_Color::Black);
		srfPlayer.SetTilesetIndex(player.vely ? 1 : (player.velx ? 3 + ((ZLTICKS / 80) % 3) : 0));
		if (player.velx > 0) srfPlayer.SetScale( PLAYER_SCALE, PLAYER_SCALE);
		if (player.velx < 0) srfPlayer.SetScale(-PLAYER_SCALE, PLAYER_SCALE);
		srfPlayer.Draw(player.x+PLAYER_WIDTH, player.y);
	}

	static float stretchT = 0;
	stretchT += ZLELAPSEDTICKS * (.001f + MIN(score_y, 100) * .0002f);
	float stretchStripes = ssin(stretchT);
	ZL_Display::FillGradient(view.left - 1, -1, 0, 100, colOutGradientTop, colOutGradientTop, colOutGradientBottom, colOutGradientBottom);
	ZL_Display::FillGradient((float)WELL_WIDTH, -1, view.right + 1, 100, colOutGradientTop, colOutGradientTop, colOutGradientBottom, colOutGradientBottom);
	srfStripes.DrawTo(view.left - 2 + stretchStripes, view.low - 1, (float)0, view.high + 1, colStripes);
	srfStripes.DrawTo(view.right + 2 - stretchStripes, view.low - 1, (float)WELL_WIDTH, view.high + 1, colStripes);

	float text_x = ZL_Display::WorldToScreen(WELL_WIDTH, 0).x;
	ZL_Display::PopOrtho();

	for (float shadow = 3.f; shadow >= 0; shadow -= 3.f)
	{
		ZL_Color col = (shadow ? ZLLUMA(0,.6) : ZLWHITE);
		txt[0].Draw(text_x + 50 + shadow, ZLFROMH(100) - shadow, col);
		txt[1].Draw(text_x + 50 + shadow, ZLFROMH(130) - shadow, col);
		txt[2].Draw(text_x + 50 + shadow, ZLFROMH(200) - shadow, col);
		txt[3].Draw(text_x + 50 + shadow, ZLFROMH(230) - shadow, col);
		txt[4].Draw(text_x + 50 + shadow, ZLFROMH(300) - shadow, col);
		txt[5].Draw(text_x + 50 + shadow, ZLFROMH(330) - shadow, col);
		fntMain.Draw(MAX(text_x + 10, ZLFROMW(200)) + shadow, 10 - shadow, "Press 'ESC' to restart", .5f, .5f, col);
	}

	if (ZLSINCE(upgradeTicks) < 500)
	{
		float t = ZL_Easing::InQuad(ZLSINCE(upgradeTicks) / 500.f);
		for (float shadow = 3.f; shadow >= 0; shadow -= 3.f)
		{
			ZL_Color col = (shadow ? ZLLUMA(0,.3) : ZLLUMA(1, .5));
			txt[3].Draw(text_x + 50 + shadow, ZLFROMH(230) - shadow - 10 + 10*t, 2 - t, 2 - t, col);
		}
	}

	if (player.dead)
	{
		ZL_Color colOuter = ZLLUMA(0, .1), colInner = ZLLUMA(1, .2);
		float t = ZL_Easing::InQuad(1.f - ZL_Math::Clamp01(ZLSINCE(deadTicks) / 1000.f));
		for (float scale = 10; scale >= 0; scale--)
			for (int i = 0; i != 9; i++)
				txtGameOver.Draw(ZLCENTER+ZLV(i/3-1,(i%3)-1)*3, 2+scale*t, 2+scale*t, colOuter, ZL_Origin::Center);
		for (float scale = 10; scale >= 0; scale--)
			txtGameOver.Draw(ZLCENTER, 2+scale*t, 2+scale*t, colInner, ZL_Origin::Center);
		if (ZLSINCE(deadTicks) > 500)
		{
			fntMain.Draw(ZLCENTER - ZLV(0, 100), "Press 'SPACE' to restart", ZLWHITE, ZL_Origin::Center);
		}
	}
}

static struct sTowerOfMinos : public ZL_Application
{
	sTowerOfMinos() : ZL_Application(60) { }

	virtual void Load(int argc, char *argv[])
	{
		if (!ZL_Application::LoadReleaseDesktopDataBundle()) return;
		if (!ZL_Display::Init("Tower of Minos", 1280, 720, ZL_DISPLAY_ALLOWRESIZEHORIZONTAL)) return;
		ZL_Display::ClearFill(ZL_Color::White);
		ZL_Display::SetAA(true);
		ZL_Audio::Init();
		ZL_Input::Init();

		fntMain = ZL_Font("Data/vipond_chubby.ttf.zip", 32);
		srfBG = ZL_Surface("Data/bg.png").SetTextureRepeatMode().SetScale(WELL_WIDTH/64.f/WELL_WIDTH);
		srfBlocks = ZL_Surface("Data/blocks.png").SetTilesetClipping(2, 2);
		srfPlayer = ZL_Surface("Data/player.png").SetTilesetClipping(3, 2).SetOrigin(ZL_Origin::BottomCenter).SetScale(PLAYER_SCALE, PLAYER_SCALE);
		srfStripes = ZL_Surface("Data/stripes.png");
		srfLudumDare = ZL_Surface("Data/ludumdare.png").SetDrawOrigin(ZL_Origin::BottomRight);

		txtGameOver = fntMain.CreateBuffer("GAME OVER");
		txtTitle = fntMain.CreateBuffer(.5f, "Tower\nof\nMinos");
		txt[0] = fntMain.CreateBuffer("Score:");
		txt[2] = fntMain.CreateBuffer("Current:");

		imcMusic.Play();
	}

	virtual void AfterFrame()
	{
		static float accumulate = 0;
		for (accumulate += ZLELAPSED; accumulate > TOMTPF; accumulate -= TOMTPF)
			Update();
		Draw();
	}
} TowerOfMinos;

#if 1 //sound and music data

// ------------------------------------------------------------------------------------------------------------------------------------------------------

static const unsigned int IMCJUMP_OrderTable[] = { 0x000000001, };
static const unsigned char IMCJUMP_PatternData[] = { 0x50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, };
static const unsigned char IMCJUMP_PatternLookupTable[] = { 0, 1, 1, 1, 1, 1, 1, 1, };
static const TImcSongEnvelope IMCJUMP_EnvList[] = {
	{ 0, 256, 69, 8, 16, 255, true, 255, },
	{ 0, 256, 184, 27, 13, 255, true, 255, },
};
static TImcSongEnvelopeCounter IMCJUMP_EnvCounterList[] = {
	{ 0, 0, 256 }, { -1, -1, 256 }, { 1, 0, 18 },
};
static const TImcSongOscillator IMCJUMP_OscillatorList[] = {
	{ 8, 0, IMCSONGOSCTYPE_SAW, 0, -1, 100, 1, 2 },
	{ 8, 106, IMCSONGOSCTYPE_SQUARE, 0, 0, 100, 1, 1 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 1, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 2, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 3, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 4, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 5, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 6, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 7, -1, 100, 0, 0 },
};
static unsigned char IMCJUMP_ChannelVol[8] = { 100, 100, 100, 100, 100, 100, 100, 100 };
static const unsigned char IMCJUMP_ChannelEnvCounter[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static const bool IMCJUMP_ChannelStopNote[8] = { true, false, false, false, false, false, false, false };
TImcSongData imcDataIMCJUMP = {
	/*LEN*/ 0x1, /*ROWLENSAMPLES*/ 2594, /*ENVLISTSIZE*/ 2, /*ENVCOUNTERLISTSIZE*/ 3, /*OSCLISTSIZE*/ 9, /*EFFECTLISTSIZE*/ 0, /*VOL*/ 70,
	IMCJUMP_OrderTable, IMCJUMP_PatternData, IMCJUMP_PatternLookupTable, IMCJUMP_EnvList, IMCJUMP_EnvCounterList, IMCJUMP_OscillatorList, NULL,
	IMCJUMP_ChannelVol, IMCJUMP_ChannelEnvCounter, IMCJUMP_ChannelStopNote };
ZL_SynthImcTrack imcJump(&imcDataIMCJUMP, false);

// ------------------------------------------------------------------------------------------------------------------------------------------------------

static const unsigned int IMCDEATH_OrderTable[] = {
	0x000000001,
};
static const unsigned char IMCDEATH_PatternData[] = {
	0x50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
static const unsigned char IMCDEATH_PatternLookupTable[] = { 0, 1, 1, 1, 1, 1, 1, 1, };
static const TImcSongEnvelope IMCDEATH_EnvList[] = {
	{ 0, 256, 15, 8, 16, 255, true, 255, },
	{ 0, 256, 14, 7, 15, 255, true, 255, },
	{ 118, 138, 1046, 8, 255, 255, true, 255, },
};
static TImcSongEnvelopeCounter IMCDEATH_EnvCounterList[] = {
	{ 0, 0, 256 }, { -1, -1, 256 }, { 1, 0, 254 }, { 2, 0, 138 },
};
static const TImcSongOscillator IMCDEATH_OscillatorList[] = {
	{ 8, 0, IMCSONGOSCTYPE_SINE, 0, -1, 100, 1, 2 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 0, 0, 72, 1, 3 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 1, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 2, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 3, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 4, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 5, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 6, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 7, -1, 100, 0, 0 },
};
static unsigned char IMCDEATH_ChannelVol[8] = { 100, 100, 100, 100, 100, 100, 100, 100 };
static const unsigned char IMCDEATH_ChannelEnvCounter[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static const bool IMCDEATH_ChannelStopNote[8] = { true, false, false, false, false, false, false, false };
TImcSongData imcDataIMCDEATH = {
	/*LEN*/ 0x1, /*ROWLENSAMPLES*/ 8268, /*ENVLISTSIZE*/ 3, /*ENVCOUNTERLISTSIZE*/ 4, /*OSCLISTSIZE*/ 9, /*EFFECTLISTSIZE*/ 0, /*VOL*/ 100,
	IMCDEATH_OrderTable, IMCDEATH_PatternData, IMCDEATH_PatternLookupTable, IMCDEATH_EnvList, IMCDEATH_EnvCounterList, IMCDEATH_OscillatorList, NULL,
	IMCDEATH_ChannelVol, IMCDEATH_ChannelEnvCounter, IMCDEATH_ChannelStopNote };
ZL_SynthImcTrack imcDeath(&imcDataIMCDEATH, false);

// ------------------------------------------------------------------------------------------------------------------------------------------------------

static const unsigned int IMCFALL_OrderTable[] = { 0x000000001, };
static const unsigned char IMCFALL_PatternData[] = { 0, 0, 0x50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, };
static const unsigned char IMCFALL_PatternLookupTable[] = { 0, 1, 1, 1, 1, 1, 1, 1, };
static const TImcSongEnvelope IMCFALL_EnvList[] = { { 0, 256, 65, 8, 16, 4, true, 255, }, { 0, 256, 43, 5, 19, 255, true, 255, }, };
static TImcSongEnvelopeCounter IMCFALL_EnvCounterList[] = { { 0, 0, 256 }, { -1, -1, 256 }, { 1, 0, 238 }, };
static const TImcSongOscillator IMCFALL_OscillatorList[] = {
	{ 9, 150, IMCSONGOSCTYPE_SINE, 0, -1, 100, 1, 2 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 1, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 2, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 3, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 4, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 5, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 6, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 7, -1, 100, 0, 0 },
};
static const TImcSongEffect IMCFALL_EffectList[] = {
	{ 92, 0, 3307, 0, IMCSONGEFFECTTYPE_DELAY, 0, 0 },
	{ 97, 105, 1, 0, IMCSONGEFFECTTYPE_RESONANCE, 1, 1 },
};
static unsigned char IMCFALL_ChannelVol[8] = { 100, 100, 100, 100, 100, 100, 100, 100 };
static const unsigned char IMCFALL_ChannelEnvCounter[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static const bool IMCFALL_ChannelStopNote[8] = { false, false, false, false, false, false, false, false };
TImcSongData imcDataIMCFALL = {
	/*LEN*/ 0x1, /*ROWLENSAMPLES*/ 6615, /*ENVLISTSIZE*/ 2, /*ENVCOUNTERLISTSIZE*/ 3, /*OSCLISTSIZE*/ 8, /*EFFECTLISTSIZE*/ 2, /*VOL*/ 70,
	IMCFALL_OrderTable, IMCFALL_PatternData, IMCFALL_PatternLookupTable, IMCFALL_EnvList, IMCFALL_EnvCounterList, IMCFALL_OscillatorList, IMCFALL_EffectList,
	IMCFALL_ChannelVol, IMCFALL_ChannelEnvCounter, IMCFALL_ChannelStopNote };
ZL_SynthImcTrack imcFall(&imcDataIMCFALL, false);

// ------------------------------------------------------------------------------------------------------------------------------------------------------

static const unsigned int IMCLAND_OrderTable[] = { 0x011000011, };
static const unsigned char IMCLAND_PatternData[] = {
	0x40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	255, 0x10, 0x40, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0x40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0x40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
static const unsigned char IMCLAND_PatternLookupTable[] = { 0, 1, 2, 2, 2, 2, 2, 3, };
static const TImcSongEnvelope IMCLAND_EnvList[] = {
	{ 0, 256, 204, 8, 16, 4, true, 255, },
	{ 0, 256, 209, 8, 16, 255, true, 255, },
	{ 64, 256, 261, 8, 15, 255, true, 255, },
	{ 0, 256, 3226, 8, 16, 255, true, 255, },
	{ 0, 256, 99, 8, 16, 16, true, 255, },
	{ 0, 256, 130, 8, 255, 255, true, 255, },
	{ 0, 386, 65, 8, 16, 255, true, 255, },
	{ 0, 256, 33, 8, 16, 255, true, 255, },
	{ 128, 256, 99, 8, 16, 255, true, 255, },
	{ 0, 128, 50, 8, 16, 255, true, 255, },
	{ 0, 256, 201, 5, 19, 255, true, 255, },
	{ 0, 256, 133, 8, 16, 255, true, 255, },
	{ 0, 256, 87, 8, 16, 255, true, 255, },
	{ 0, 256, 228, 8, 16, 255, true, 255, },
	{ 0, 256, 444, 8, 16, 255, true, 255, },
	{ 0, 256, 627, 23, 15, 255, true, 255, },
	{ 0, 256, 174, 8, 16, 255, true, 255, },
	{ 256, 271, 65, 8, 16, 255, true, 255, },
	{ 0, 512, 11073, 0, 255, 255, true, 255, },
};
static TImcSongEnvelopeCounter IMCLAND_EnvCounterList[] = {
	{ 0, 0, 256 }, { 1, 0, 256 }, { 2, 0, 256 }, { -1, -1, 256 },
	{ 3, 0, 256 }, { 4, 1, 256 }, { 5, 1, 256 }, { 6, 6, 386 },
	{ 7, 6, 256 }, { 8, 6, 256 }, { 9, 6, 128 }, { -1, -1, 258 },
	{ 10, 6, 238 }, { 11, 6, 256 }, { 12, 7, 256 }, { 13, 7, 256 },
	{ 14, 7, 256 }, { -1, -1, 384 }, { 15, 7, 0 }, { 16, 7, 256 },
	{ 17, 7, 271 }, { 18, 7, 256 },
};
static const TImcSongOscillator IMCLAND_OscillatorList[] = {
	{ 6, 127, IMCSONGOSCTYPE_SINE, 0, -1, 206, 1, 2 },
	{ 8, 0, IMCSONGOSCTYPE_NOISE, 0, -1, 186, 4, 3 },
	{ 7, 0, IMCSONGOSCTYPE_NOISE, 0, 0, 152, 3, 3 },
	{ 6, 66, IMCSONGOSCTYPE_SINE, 1, -1, 244, 3, 3 },
	{ 8, 0, IMCSONGOSCTYPE_NOISE, 1, 3, 160, 6, 3 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 2, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 3, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 4, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 5, -1, 100, 0, 0 },
	{ 4, 227, IMCSONGOSCTYPE_SINE, 6, -1, 255, 8, 9 },
	{ 9, 15, IMCSONGOSCTYPE_NOISE, 6, -1, 255, 10, 11 },
	{ 4, 150, IMCSONGOSCTYPE_SINE, 6, -1, 255, 12, 3 },
	{ 5, 174, IMCSONGOSCTYPE_SINE, 6, -1, 230, 13, 3 },
	{ 6, 238, IMCSONGOSCTYPE_SINE, 7, -1, 0, 15, 3 },
	{ 5, 66, IMCSONGOSCTYPE_SINE, 7, -1, 134, 16, 17 },
	{ 7, 127, IMCSONGOSCTYPE_NOISE, 7, -1, 0, 18, 3 },
	{ 8, 0, IMCSONGOSCTYPE_NOISE, 7, -1, 0, 3, 3 },
	{ 6, 106, IMCSONGOSCTYPE_SINE, 7, -1, 142, 19, 20 },
	{ 5, 200, IMCSONGOSCTYPE_SAW, 7, -1, 104, 3, 3 },
	{ 8, 0, IMCSONGOSCTYPE_NOISE, 7, 14, 212, 3, 3 },
	{ 7, 0, IMCSONGOSCTYPE_NOISE, 7, 18, 228, 3, 21 },
};
static const TImcSongEffect IMCLAND_EffectList[] = {
	{ 9906, 843, 1, 0, IMCSONGEFFECTTYPE_OVERDRIVE, 0, 3 },
	{ 142, 51, 1, 0, IMCSONGEFFECTTYPE_RESONANCE, 3, 3 },
	{ 227, 0, 1, 6, IMCSONGEFFECTTYPE_LOWPASS, 3, 0 },
	{ 103, 218, 1, 6, IMCSONGEFFECTTYPE_RESONANCE, 3, 3 },
	{ 13716, 109, 1, 6, IMCSONGEFFECTTYPE_OVERDRIVE, 0, 3 },
	{ 97, 206, 1, 7, IMCSONGEFFECTTYPE_RESONANCE, 3, 3 },
	{ 154, 0, 1, 7, IMCSONGEFFECTTYPE_LOWPASS, 3, 0 },
};
static unsigned char IMCLAND_ChannelVol[8] = { 71, 25, 100, 100, 100, 100, 69, 179 };
static const unsigned char IMCLAND_ChannelEnvCounter[8] = { 0, 5, 0, 0, 0, 0, 7, 14 };
static const bool IMCLAND_ChannelStopNote[8] = { false, false, false, false, false, false, true, true };
TImcSongData imcDataIMCLAND = {
	/*LEN*/ 0x1, /*ROWLENSAMPLES*/ 3575, /*ENVLISTSIZE*/ 19, /*ENVCOUNTERLISTSIZE*/ 22, /*OSCLISTSIZE*/ 21, /*EFFECTLISTSIZE*/ 7, /*VOL*/ 70,
	IMCLAND_OrderTable, IMCLAND_PatternData, IMCLAND_PatternLookupTable, IMCLAND_EnvList, IMCLAND_EnvCounterList, IMCLAND_OscillatorList, IMCLAND_EffectList,
	IMCLAND_ChannelVol, IMCLAND_ChannelEnvCounter, IMCLAND_ChannelStopNote };
ZL_SynthImcTrack imcLand(&imcDataIMCLAND, false);

// ------------------------------------------------------------------------------------------------------------------------------------------------------

static const unsigned int IMCLVLUP_OrderTable[] = {
	0x000000001,
};
static const unsigned char IMCLVLUP_PatternData[] = {
	0x50, 0x52, 0x54, 0x55, 0x5B, 0x60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
static const unsigned char IMCLVLUP_PatternLookupTable[] = { 0, 1, 1, 1, 1, 1, 1, 1, };
static const TImcSongEnvelope IMCLVLUP_EnvList[] = {
	{ 0, 256, 145, 8, 16, 255, true, 255, },
};
static TImcSongEnvelopeCounter IMCLVLUP_EnvCounterList[] = {
	{ 0, 0, 256 }, { -1, -1, 256 },
};
static const TImcSongOscillator IMCLVLUP_OscillatorList[] = {
	{ 8, 0, IMCSONGOSCTYPE_SINE, 0, -1, 255, 1, 1 },
	{ 10, 0, IMCSONGOSCTYPE_SINE, 0, 0, 255, 1, 1 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 2, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 3, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 4, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 5, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 6, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 7, -1, 100, 0, 0 },
};
static const TImcSongEffect IMCLVLUP_EffectList[] = {
	{ 117, 0, 4960, 0, IMCSONGEFFECTTYPE_DELAY, 0, 0 },
	{ 255, 0, 1, 0, IMCSONGEFFECTTYPE_HIGHPASS, 1, 0 },
};
static unsigned char IMCLVLUP_ChannelVol[8] = { 97, 97, 100, 100, 100, 100, 100, 100 };
static const unsigned char IMCLVLUP_ChannelEnvCounter[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static const bool IMCLVLUP_ChannelStopNote[8] = { true, true, false, false, false, false, false, false };
TImcSongData imcDataIMCLVLUP = {
	/*LEN*/ 0x1, /*ROWLENSAMPLES*/ 3307, /*ENVLISTSIZE*/ 1, /*ENVCOUNTERLISTSIZE*/ 2, /*OSCLISTSIZE*/ 8, /*EFFECTLISTSIZE*/ 2, /*VOL*/ 150,
	IMCLVLUP_OrderTable, IMCLVLUP_PatternData, IMCLVLUP_PatternLookupTable, IMCLVLUP_EnvList, IMCLVLUP_EnvCounterList, IMCLVLUP_OscillatorList, IMCLVLUP_EffectList,
	IMCLVLUP_ChannelVol, IMCLVLUP_ChannelEnvCounter, IMCLVLUP_ChannelStopNote };
ZL_SynthImcTrack imcLvlUp(&imcDataIMCLVLUP, false);

// ------------------------------------------------------------------------------------------------------------------------------------------------------

static const unsigned int IMCMUSIC_OrderTable[] = {
	0x001000001, 0x011000002, 0x021000001, 0x011000002, 0x021000016, 0x011000023, 0x021000034, 0x011000023,
	0x021000005, 0x011000023, 0x021000034, 0x001000023, 0x001000005, 0x001000000,
};
static const unsigned char IMCMUSIC_PatternData[] = {
	0x57, 0, 0, 0, 0x57, 0, 0x54, 0, 0, 0, 0x55, 0, 0, 0, 0, 0,
	0x57, 0, 0, 0, 0x57, 0, 0x54, 0, 0, 0, 0x55, 0, 0, 0, 0x57, 0,
	0x60, 0, 0x5B, 0, 0x59, 0, 0, 0, 0, 0, 0x59, 0, 0x59, 0, 0, 0,
	0x5B, 0, 0x59, 0, 0x54, 0, 0, 0, 0, 0, 0x54, 0, 0x54, 0, 0, 0,
	0x5B, 0, 0x59, 0, 0x54, 0, 0x57, 0, 0x59, 0, 0x59, 0, 0, 0, 0, 0,
	0x57, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x60, 0,
	0x50, 0, 0x52, 0, 0x54, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0x60, 0, 0x5B, 0, 0x59, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0x5B, 0, 0x59, 0, 0x54, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0x50, 0, 0, 0, 0x50, 0, 0, 0, 0x50, 0, 0, 0, 0x50, 0,
	0x50, 0, 0, 0, 0x50, 0, 0, 0, 0x50, 0, 0, 0, 0x50, 0, 0, 0,
	0x50, 0, 0x52, 0, 0x50, 0, 0, 0, 0, 0, 0x45, 0, 0x52, 0, 0x50, 0,
};
static const unsigned char IMCMUSIC_PatternLookupTable[] = { 0, 6, 9, 9, 9, 9, 9, 10, };
static const TImcSongEnvelope IMCMUSIC_EnvList[] = {
	{ 0, 256, 58, 6, 18, 255, true, 255, },
	{ 0, 256, 152, 8, 16, 255, true, 255, },
	{ 0, 256, 9, 8, 255, 255, false, 3, },
	{ 0, 256, 28, 8, 16, 255, true, 255, },
	{ 0, 256, 24, 24, 16, 255, true, 255, },
	{ 0, 256, 523, 8, 16, 255, true, 255, },
	{ 32, 256, 196, 8, 16, 255, true, 255, },
	{ 0, 256, 204, 8, 16, 4, true, 255, },
	{ 0, 256, 209, 8, 16, 255, true, 255, },
	{ 64, 256, 261, 8, 15, 255, true, 255, },
	{ 0, 256, 523, 8, 15, 255, true, 255, },
	{ 0, 256, 2092, 8, 16, 255, true, 255, },
};
static TImcSongEnvelopeCounter IMCMUSIC_EnvCounterList[] = {
	{ 0, 0, 248 }, { -1, -1, 256 }, { 1, 0, 256 }, { 2, 0, 256 },
	{ 3, 1, 256 }, { 4, 1, 0 }, { -1, -1, 128 }, { 4, 1, 0 },
	{ 5, 6, 256 }, { 6, 6, 256 }, { 7, 7, 256 }, { 8, 7, 256 },
	{ 9, 7, 256 }, { 8, 7, 256 }, { 10, 7, 256 }, { 11, 7, 256 },
};
static const TImcSongOscillator IMCMUSIC_OscillatorList[] = {
	{ 8, 0, IMCSONGOSCTYPE_SINE, 0, -1, 100, 1, 1 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 0, -1, 66, 1, 1 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 0, -1, 24, 1, 1 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 0, -1, 88, 2, 1 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 0, 1, 36, 1, 1 },
	{ 8, 0, IMCSONGOSCTYPE_NOISE, 0, 3, 48, 1, 1 },
	{ 6, 254, IMCSONGOSCTYPE_SQUARE, 1, -1, 255, 1, 1 },
	{ 7, 0, IMCSONGOSCTYPE_SQUARE, 1, -1, 168, 5, 1 },
	{ 7, 0, IMCSONGOSCTYPE_SQUARE, 1, -1, 148, 7, 1 },
	{ 6, 254, IMCSONGOSCTYPE_SAW, 1, -1, 136, 1, 1 },
	{ 7, 0, IMCSONGOSCTYPE_SAW, 1, 6, 20, 1, 1 },
	{ 2, 31, IMCSONGOSCTYPE_SINE, 1, 7, 2, 6, 6 },
	{ 2, 31, IMCSONGOSCTYPE_SINE, 1, 8, 2, 6, 6 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 2, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 3, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 4, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 5, -1, 100, 0, 0 },
	{ 8, 0, IMCSONGOSCTYPE_NOISE, 6, -1, 127, 1, 9 },
	{ 5, 227, IMCSONGOSCTYPE_SINE, 7, -1, 114, 11, 12 },
	{ 8, 0, IMCSONGOSCTYPE_SINE, 7, -1, 12, 13, 14 },
	{ 8, 0, IMCSONGOSCTYPE_NOISE, 7, -1, 116, 15, 1 },
	{ 7, 0, IMCSONGOSCTYPE_NOISE, 7, 18, 10, 1, 1 },
};
static const TImcSongEffect IMCMUSIC_EffectList[] = {
	{ 191, 247, 1, 0, IMCSONGEFFECTTYPE_RESONANCE, 1, 3 },
	{ 124, 0, 1, 0, IMCSONGEFFECTTYPE_LOWPASS, 1, 0 },
	{ 253, 121, 1, 1, IMCSONGEFFECTTYPE_RESONANCE, 1, 1 },
	{ 112, 0, 18039, 1, IMCSONGEFFECTTYPE_DELAY, 0, 0 },
	{ 112, 0, 36078, 1, IMCSONGEFFECTTYPE_DELAY, 0, 0 },
	{ 28, 0, 6013, 6, IMCSONGEFFECTTYPE_DELAY, 0, 0 },
	{ 255, 156, 1, 6, IMCSONGEFFECTTYPE_RESONANCE, 1, 1 },
	{ 227, 0, 1, 6, IMCSONGEFFECTTYPE_HIGHPASS, 1, 0 },
	{ 7239, 1154, 1, 7, IMCSONGEFFECTTYPE_OVERDRIVE, 0, 1 },
	{ 21, 107, 1, 7, IMCSONGEFFECTTYPE_RESONANCE, 1, 1 },
};
static unsigned char IMCMUSIC_ChannelVol[8] = { 153, 46, 100, 100, 100, 100, 135, 122 };
static const unsigned char IMCMUSIC_ChannelEnvCounter[8] = { 0, 4, 0, 0, 0, 0, 8, 10 };
static const bool IMCMUSIC_ChannelStopNote[8] = { true, true, false, false, false, false, true, false };
TImcSongData imcDataIMCMUSIC = {
	/*LEN*/ 0xE, /*ROWLENSAMPLES*/ 6013, /*ENVLISTSIZE*/ 12, /*ENVCOUNTERLISTSIZE*/ 16, /*OSCLISTSIZE*/ 22, /*EFFECTLISTSIZE*/ 10, /*VOL*/ 40,
	IMCMUSIC_OrderTable, IMCMUSIC_PatternData, IMCMUSIC_PatternLookupTable, IMCMUSIC_EnvList, IMCMUSIC_EnvCounterList, IMCMUSIC_OscillatorList, IMCMUSIC_EffectList,
	IMCMUSIC_ChannelVol, IMCMUSIC_ChannelEnvCounter, IMCMUSIC_ChannelStopNote };
ZL_SynthImcTrack imcMusic(&imcDataIMCMUSIC, true);

// ------------------------------------------------------------------------------------------------------------------------------------------------------

#endif //sound and music data
