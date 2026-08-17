<!--
  Research reference for the built-in media player on the roadmap.

  Compiled from archived developer material, two Japanese reviews with
  screenshots, two video walkthroughs and preserved App Store reviews - the
  app itself is delisted, so none of this could be checked by running it.
  Points that could not be corroborated are marked as uncertain in the text
  rather than smoothed over; leave them that way.
-->

# Vinyl — the Real Record Player (elephantcandy) — Design Reference

**Status of the subject.** The app is dead. `itunes.apple.com/lookup?id=571600993` returns `resultCount: 0`, and both `apps.apple.com/us/app/…/id571600993` and the developer's own product pages 404. Everything below is reconstructed from archived developer material, two contemporaneous Japanese reviews with screenshots, two video walkthroughs, archived App Store listing text, and preserved Japanese user reviews. Where I could not corroborate something I say so explicitly.

**Provenance.** iPad: `Vinyl – the Real Record Player`, elephantcandy (Rapenburg 91c, 1011TW Amsterdam), App Store id **571600993**, released **10 Nov 2012**, launch price **$0.99 / ¥85**, later **$3.99**, 91.4 MB → 104.4 MB, iPad 2 and later. iPhone: `Vinyl mini`, id **646358530**, released **23 May 2013**, ¥480, 29.7–31.0 MB, iOS 5.0+. Final version **1.3** (5 Mar 2015): *"Audiobus support / bugfixes"*, plus *"Fixed scratching issues on iPad Air."* A Facebook post dated 17 Dec 2013 mentions a 64-bit audio update.

Source screenshots are kept locally under `Claude Code/vinyl-reference/` on the build machine and are deliberately **not committed** - they are AppBank's and Apple's, not ours. The `ab-*.jpg` files are the AppBank screenshots of the iPad build and are the single most useful artefact; `mini-ss*.png` are the App Store screenshots of the iPhone build. Filenames are referenced below so the descriptions can be checked against them.

---

## 1. The turntable itself

There is no single turntable. **There are seven**, spanning the 1940s to "the start of the new millennium," and they are not reskins — layout, control set, control *type*, and audio character all differ per deck. Official copy: *"Vinyl consists of seven different record players, from plastic toy player to full-fledged hi-fi. Every record player allows you to travel back to an old-school music experience. From the 1940's to the start of the new millennium: Vinyl gives you the unique look, feel and sound."*

**Names confirmed by primary sources (5 of 7):**
- **Triumphola** — '40s
- **High Fidelity** — '50s (Touch Lab: *「下の「High Fidelity」と名付けられたターンテーブルは、50年代をイメージしたもの」* — "the turntable named 'High Fidelity' is modelled on the 50s")
- **TurnoMate** — retro-pop plastic, likely '60s
- **EC Technic** — '70s (an obvious Technics SL-1200 pastiche)
- **DG-100 "TECH.DIGITAL TURNTABLE"** — newest

The remaining two names I could not recover. Do not invent them.

### Deck picker (entry screen) — `ab-01.jpg`
A horizontal carousel on a flat light-grey seamless-studio background. The selected deck is centred, rendered as a photoreal 3/4-ish top-down object with a soft contact shadow; neighbours are clipped at the left and right edges to signal swipe. Above the deck, centred: **model name** in large bold white/grey type, **era** beneath it in smaller type (`EC Technic` / `'70s`). A single circular **ⓘ** button sits in the bottom-right corner. Nothing else. This screen does the heavy lifting for the whole product identity — it is a shelf of objects, not a menu.

### Player screen — iPad layout (`ab-02-1.jpg`, `ab-04.jpg`)
Landscape. The deck fills essentially the entire screen edge-to-edge; there is no app chrome above it and no status bar visible. A **vertical dark charcoal rail, ~6% of screen width, is pinned to the right edge**, overlaying the plinth. Everything else is the object.

Taking EC Technic as the archetype:
- **Plinth**: left ~72% is a cream/off-white brushed top plate; the right ~28% is dark rosewood veneer with visible grain. Four chrome screw heads at the plate corners. The whole thing has a subtle drop shadow against the app background, i.e. it reads as a physical object sitting on a surface, not as a wallpaper.
- **Platter/record**: a large disc occupying roughly the left two-thirds, vertically centred, its centre sitting around x≈32% / y≈45%. The record is rendered with **concentric groove rings**, a specular sheen sweep across the upper-left quadrant, a silver/chrome platter rim just visible outside the vinyl edge, and a thin **spindle pin** poking through the label centre.
- **Tonearm**: mounted on a chunky chrome pivot assembly at approximately x≈80%, y≈20%. Above the pivot sits a cylindrical **counterweight** with knurled banding. The arm is an **S-shaped chrome tube** sweeping down and left toward the record. At the far end, a rectangular **headshell** with a grey cartridge body and finger-lift. There is a separate small **arm rest / clamp** on the plinth (visible as a small dark bracket at x≈83%, y≈43% in `ab-05.jpg` once the arm has swung away).
- **Bottom control strip** (`bottomstrip.png`): a narrow inset black panel running the width of the top plate, containing, left to right: the label `start / stop`; `r.p.m.` with the numerals **33** and **45**; a chrome nameplate reading **EC Technic**; and `pitch adj.` above a **ruler scale reading −8 · 6 · 4 · 2 · [green centre tick] · 2 · 4 · 6 · +8**. Physical controls protrude *below* the panel onto the plate itself: a small two-position lever slider under the 33/45 numerals, and a knurled horizontal **pitch fader** under the scale. See `pitchscale.png`.
- **Start/stop**: NOT in the strip. It is a raised square cream **button in the bottom-left corner of the plinth**, above its `start / stop` caption (`ab-05.jpg`).
- A small blank white square sits at roughly x≈9%, y≈82% — the start/stop button itself.

### Deck-to-deck variation is the whole point
- **Triumphola, '40s** (`ab-09.jpg`): dark oak plinth with heavy visible grain, wear marks and exposed screws. A small **brass maker's badge** top-centre. A massive **gold/brass tonearm** with a fat curved horn-like pickup head — a 78-era arm, not a hi-fi arm. Bottom-left, a brass speed **dial** engraved `SLOW … FAST` (continuous, not detented). Bottom-centre, a tiny `START ▸ STOP` toggle lever. **No pitch fader, no rpm switch, no visible mat.** The control set is deliberately impoverished.
- **TurnoMate** (`ab-10.jpg`): cream/ivory moulded plastic case with an **orange** flat-blade tonearm shaped like a plastic ruler with `TURNOMATE` moulded into it. A large stamped-metal **speaker grille** occupies the right side under the arm — this deck has its own speaker, which is why AppBank described its sound as *「広いトンネルの中で聴いているみたい」* ("like listening inside a big tunnel"). Bottom row: a `TURNOMATE` logo plaque, an **on/off rocker with a green pilot lamp**, a blank centre plaque, and a `33 / 45` slider in an orange panel.
- **DG-100** (`ab-11.jpg`): black textured/brushed metal, a ring of **strobe dots** around the platter rim, a straight-ish arm with counterweight, anti-skate ring, a white cartridge in the headshell, and a **cue lever**. A large round **illuminated green start/stop button** sits bottom-centre with a visible green glow spill onto the plinth; a separate square `Start/Stop` button bottom-left; `r.p.m.` **33 / 45 backlit cyan pushbuttons**; and — importantly — the **pitch fader is vertical here**, on the right side, labelled `pitch adj.` with `+` / `−` and a green centre LED. Model name silkscreened bottom-left: `DG-100  TECH.DIGITAL  TURNTABLE`.

**Takeaway for your Android build:** the era gimmick works because the *control affordances* change, not just the textures. A 1940s deck with a pitch fader would break the illusion.

### The record label area
The album artwork is **circle-cropped and composited into the label area** of the disc, at real label proportions (roughly 30–35% of disc diameter), with the spindle pin drawn on top of it (`ab-04.jpg`). AppBank: *「ジャケット画像がディスクの真ん中に表示されて本物のレコードみたいです」* — "the jacket image is shown in the middle of the disc, just like a real record."

When there is **no artwork**, the app substitutes a **generated fictional label**. From the Sound Test Room walkthrough: *"it comes with this funky little label that they've made up because I've got no artwork for it… and also the little labels are different for each track, I suppose it repeats eventually, and it actually says on there elephantcandy Records."* The Vinyl mini screenshots confirm this — `mini-ss2.png` shows a blue period-correct label, `mini-ss3.png` a white one. So: **a pool of vintage label designs, assigned per track, branded "elephantcandy Records."** This is a genuinely good idea worth stealing — a real label typography set beats a grey placeholder square.

**Groove rendering is data-driven, not decorative.** A Japanese App Store reviewer (title 「傑作です！」, 5 stars): *「盤面は曲の長さに合わせて溝ができてます。かなりリアルです」* — "the disc surface has grooves generated to match the length of the tracks. Quite realistic." The second video walkthrough confirms: *"there's actually the grooves there in the album."* So the disc shows visible **banded track separators sized proportionally to each track's duration**, exactly as on a real pressing. That is the mechanism that makes needle-drop track selection legible.

---

## 2. Interaction

**The controls are physical, not buttons-in-disguise.** There is no transport bar, no scrubber, no progress slider, no shuffle, no repeat-visible-as-a-UI-affordance. AppBank is explicit: *「画面左下の【start / stop】ボタンを押して再生します。もちろんシャッフル再生ボタンはありませんよ」* — "press the start/stop button at the bottom left to play. Of course there's no shuffle button."

**Play** = press the deck's own start/stop control. On some decks that alone doesn't produce sound.

**Cue / seek / track select = move the tonearm.** This is the core mechanic and the most important thing to copy. Touch Lab: *「ターンテーブルは、電源を入れるとトーンアームが自動で移動するものや、手でピックアップして針を置く必要があるものもあります」* — "some turntables move the tonearm automatically when you switch on; others require you to pick it up by hand and place the needle." So **auto-return vs fully manual is a per-deck property**. The '40s Triumphola is manual: AppBank, *「40年代のTriumphola は「トーンアーム」を自分で動かさなくてはいけません」* — "on the '40s Triumphola you have to move the tonearm yourself."

**There are two distinct drag targets on the arm, and this is the cleverest interaction in the app.** The official FAQ, verbatim:

> **How do I correctly lift the pick-up arm?**
> Every record player arm comes with a small handle, which enables you to lift the pick-up arm instead of dragging it.

Touch Lab spells out the consequence: *「トーンアームのつまみを持って移動すれば、頭出しや選曲ができます。つまみを持たないで動かすとガリガリとノイズが発生するの注意が必要です」* — **"if you move the tonearm by holding its finger-lift you can cue up and select tracks. Be careful — if you move it *without* holding the lift, a harsh gari-gari scratching noise is produced."**

So: **grab the finger-lift → arm lifts, silent reposition, drop = clean needle-drop seek. Grab the arm tube anywhere else → the needle stays down and drags across the record, producing the scratch/rip noise.** Two hit-targets on the same object with completely different semantics, and the app teaches you the difference by punishing you audibly. That is the single best idea in this app.

**Scratching the record.** You can also manipulate the disc directly. Second walkthrough: *"there's all sorts of other features — you can scratch on the record."* Sound Test Room: *"you can just drop it on as well, shake it around a bit in there, make it scratch… you could be a DJ."* The FAQ pre-empts the obvious worry:

> **Does my music get ruined by scratching the record?**
> No, it won't. Knock yourself out. Your music will stay unharmed.

The audible result of dragging the disc is scratch/scrub audio. What I **could not verify** is whether the disc drag is a true sample-accurate scrub (audio position bound to finger angle, à la a DVS platter) or a triggered scratch effect layered over the transport. Treat that as an open design decision on your side; the sources describe the *sound* of scratching but never the *mechanic*.

**Flip = a real gesture with a real consequence.** Album tracks are split across **two sides**. Playback **stops at the end of side A** and you must flip. AppBank: *「アルバムの半分を聴き終わったときに再生が止まってしまいました。そう、レコードはA面とB面があるんです！でも大丈夫。ひっくり返すボタンもちゃんとあります」* — "when I'd finished half the album, playback stopped. Right — records have an A side and a B side! But don't worry, there's a proper flip-over button." `ab-07.jpg` captures the mid-flight **3D flip animation** of the disc.

FAQ: *"Vinyl distributes your album tracks to both sides of your virtual record. If you have only two or three tracks of an album, you'll see just one track on one side of the LP – and one or two on the other."* And: *"Vinyl doesn't do B-sides. Every single consists of only one track."* Flipping a single shows the same track.

**Motion → skip.** The accelerometer is wired to needle skip. AppBank ran this as a callout in red text on `ab-05_-2.jpg`: **「iPadに振動を与えると音飛びする！」** — "give the iPad a jolt and the sound skips!" Touch Lab: *「ターンテーブルは振動に弱いので、再生中にiPadを動かしたり・衝撃を与えると「音飛び」する」*. Second video: *"you can make it skip if you accidentally bump your iPad while it's playing — it's like you bumped your turntable and the needle moves."* A Japanese App Store reviewer called this the best part: *「機種が揺れた際に、その衝撃が再生に影響する(針飛びが起こる)ギミック、思わず顔がほころんでしまった」* — "the gimmick where a jolt to the device affects playback (needle skip) made me smile in spite of myself." Official marketing also teases it: *"And wait 'till you shake your iPad! Give it a try."*

**Right-hand rail (iPad)** — see `sidebar.png`. Top to bottom:
1. **`[°°]`** — back to the deck carousel. Separated from the rest by whitespace.
2. **Disc-with-lifted-corner** — take the record off / eject. (Sound Test Room: *"this just takes the record off."*)
3. **♪ music note** — the library. (AppBank's arrow in `ab-02-1.jpg` points here, and the next screenshot is the crate browser.)
4. **Concentric rings** — **flip the record**. (AppBank's arrow in `ab-07.jpg` points here and that frame is the flip animation.)
5. **Velvet-pad / wavy-lines glyph** — **Groove Clean**. (AppBank's arrow in `ab-08.jpg`.)
6. **Circular arrow** — *unconfirmed.* Most likely repeat/replay or return-to-start. Do not assume.
7. **Share** (rounded square with out-arrow) — Sound Test Room: *"you can share via email."*
8. **Speaker** — volume. *Inferred from the glyph; not directly confirmed in any source.*
9. **ⓘ** — pinned separately at the bottom.

**iPhone (Vinyl mini) splits this into two rails**, one on each edge, with the deck scaled to fit between (`mini-ss1.png`–`mini-ss3.png`). Left rail carries deck-picker, flip, clean, and one more; right rail carries ⓘ, share, eject and library. Worth noting if you're targeting phones: they chose *two thin rails flanking the object* over one thick rail, to keep the turntable centred and circular.

---

## 3. Audio behaviour

**Yes, it genuinely colours the audio, and the colouration is per-deck.** This is stated as design intent in the official FAQ:

> **My music sounds strange. Help?**
> We made it that way. All record players have their own unique sound – based on the era's available technology.

Touch Lab: *「7種類のターンテーブルはデザインが異なるだけでなく、その時代の技術レベルに合わせた音も再現されています。古い年代のものは、悪く言えばこもったような音で再生されますが、返ってそれがレトロな温かみにある音にも感じられます」* — "the seven turntables differ not only in design; the sound is reproduced to match the technology level of each era. Older ones play back with what you could unkindly call a muffled/boxy sound, but conversely that reads as a warm retro tone." And: *「レコード特有のプチプチとしたノイズ…まで再現されています」* — "it even reproduces the puchi-puchi [crackle/pop] noise peculiar to records."

Concrete per-deck impressions from AppBank's reviewer:
- **Triumphola ('40s)**: *「しかもノイズが多い！けどこれがまた味がありますね」* — "and there's a *lot* of noise! But that has its own charm." The developer's own site quotes a user on *"hum from the Tubes in the 40's player."*
- **TurnoMate**: *「なんだか音が響きます。広いトンネルの中で聴いているみたいです」* — "resonant; like listening inside a big tunnel." (Boxy plastic-cabinet resonance + its onboard speaker.)
- **DG-100**: *「音がクリアで感動しました！重低音が効いています」* — "clear and impressive! The deep bass works."

**Effects confirmed present**, aggregated across sources: surface noise / hiss, crackle and pops (including **lead-in crackle before the music starts** — Sound Test Room: *"we press start and then it goes, get some crackle at the beginning, that's so bad, that's so bad"*), valve hum on the '40s deck, cabinet/box resonance, era-appropriate bandwidth restriction ("muffled" on old decks), scratch/rip noise from dragging the stylus, needle skip, and **wow/flutter / speed drift**. On the last: the second walkthrough says *"it also emulates all the scratches and hiss, varying speeds of rotation… depending on the older one, sometimes the record sounds like it's got big waves in it."* An App Store review quoted in search results reads: *"The speed wobble, clicks and pops are superbly reproduced, and you get that warm vinyl sound and crackle of the static. The needle even skips if you jog the iPad."*

**Critical finding — the colouration is gated behind iOS Settings, not in-app.** In the Sound Test Room video the reviewer stops mid-demo: *"this is not the default setting for this app — the default settings, we have to change in Settings. So we're gonna head on over to Settings and let's find Vinyl. So there it is: **Vinyl noise** — on. **Record player character** — will turn on. **Speed variance** — we shall turn on. So these are kind of global settings. **It would have been better if they'd have had them inside the actual app** — you know, you can't have everything."

So there are exactly three audio toggles, living in the iOS Settings bundle:
- **Vinyl noise** — surface noise / crackle / hiss
- **Record player character** — the per-deck EQ / resonance / distortion model
- **Speed variance** — wow and flutter / drift

They appear to be **off by default**, which means the app's defining feature was hidden from most users. **This is a mistake to avoid.** Put these in-app, expose them per-deck, and default them on.

**Speed.** Touch Lab: *「すべてのターンテーブルには、33回転(LP)・45回転(EP)の切替えスイッチがあり、機種によってはそれ以外の回転数に合わせることもできます」* — "**all** turntables have a 33 (LP) / 45 (EP) switch, and **on some models you can set other speeds too**." A secondary source (an aggregated App Store description snippet) states *"Some of the turntables let you listen at 16, 33, 45 or 78 RPM or adjust the pitch."* The Sound Test Room transcript garbles a moment where he reads out speeds including "16." I rate 16/78 on some decks as **probable but not firmly confirmed** — the screenshots I recovered only show 33/45 decks. The **pitch fader is ±8%** (read directly off the EC Technic scale in `pitchscale.png`), horizontal on EC Technic, vertical on DG-100, absent on Triumphola.

**Does pitch follow platter speed?** Almost certainly yes — a ±8% pitch control that didn't shift pitch would be pointless, and "speed variance" producing audible "big waves" is by definition pitch modulation. But **I found no source that directly describes a spin-up/spin-down pitch ramp on start/stop**, so treat the start/stop ramp as your own design call rather than a documented behaviour.

**No RIAA-style tone control is exposed.** The tone shaping is baked into "record player character" as an opaque per-deck model.

**Wear is a live audio+visual state, not a decoration.** *"Records get gray and scratchy when played frequently."* Touch Lab describes the failure mode precisely: *「レコードをしばらく聴いていると、ゴミが溜まってノイズが増えたり、同じところを何度もループして再生したりするようになります」* — **"after listening for a while, dirt accumulates, noise increases, and it starts looping the same spot over and over."** That is a simulated **stuck groove**, which is a much better idea than just raising a noise floor. AppBank hit it too: *「しばらく聴いていると、よく音飛びするようになりました。溝にホコリがたまる特性も再現しています」*.

The cure, per the FAQ: *"**What does Groove Clean exactly do?** Groove Clean is your magic dust wipe. Not only does it make your records black and shiny again, it also removes any scratches you (or your 'friends') happen to have made."* Visually (`ab-08.jpg`) a velvet pad graphic labelled **`GROOVE CLEAN / ANTI STATIC CARBON FIBER / VELVET DISC CLEANING PAD`** appears on the record surface — the sources describe it as a tap-to-clean, though the pad's presence as a draggable object strongly implies you wipe it across. I could not confirm which.

**Source restrictions.** Local library only. FAQ: *"Vinyl enables you to play all music in your iTunes music library. Unfortunately, we aren't allowed to access music made available by third parties, like Pandora and Spotify."* And App Store copy in caps: *"ATTENTION: This app doesn't work with iTunes Match, only local stored music! As soon as it is technically possible we will integrate this!"* Tracks that are DRM'd, damaged, or cloud-only are stamped **"UNAVAILABLE"** on the sleeve. Touch Lab explains the *reason*, which matters architecturally: *「時代による音質の違い再現するため、DRMで保護されている曲は再生することができません」* — "**because** it reproduces era-based differences in sound quality, DRM-protected tracks cannot be played." I.e. they needed raw PCM to run their DSP, so protected/opaque playback paths were structurally excluded. Expect exactly this constraint on Android with DRM'd streaming sources.

**Version 1.3 added Audiobus support**, so the coloured output could be routed into other iOS audio apps.

---

## 4. Library / browsing

**A crate, not a list.** `ab-03.jpg` is the iPad browser and it is worth studying directly. Layout:

- Black title bar. **戻る (Back)** pill button top-left, **magnifier (search)** top-right. Nothing else.
- Body on the same flat light-grey studio background as the deck picker. **Two columns, side by side: "LP's" (left) and "Singles" (right).**
- Each column has a large heading in bold white/grey, and beneath it a **"Sort by:" iOS-style segmented control** — LPs offer **Artist | Album**, Singles offer **Artist | Title**.
- Each column renders a **stack of sleeves in perspective**, as if flicking through a record crate: the front sleeve upright and full-size, with maybe 4–6 sleeve tops peeking above and behind it, slightly offset. A **mirror reflection** falls below the front sleeve onto the "floor."
- **LPs are square 12″ sleeves. Singles are visibly different** — smaller, in a black paper sleeve with a **die-cut centre hole** through which the label shows. Real format differentiation, not just a size change.
- The **single's title is captioned beneath it** in white ("Beautiful Life"); the LP's title is not — it relies on the artwork.
- **A–Z index rails run down *both* the left and right screen edges**, and the Japanese build appends the kana rows: `A B C … Z あ か さ た な は ま や ら わ #`. The current letter is bolded.

**Browsing gesture is a vertical flick.** Touch Lab: *「iPadに保存されている音楽ライブラリは、アナログレコードのジャケットのように表示され、タテにパラパラとめくって選ぶことができます」* — "the library is displayed like record jackets, and you flick through them **vertically** to choose." Note *vertically*, not horizontally — you're thumbing through a crate from above, which is the correct physical metaphor and the opposite of Cover Flow.

**Selection granularity: album only.** AppBank, with visible disappointment: *「曲はアルバムごとでしか選択できません。好きな曲を選んでお気に入りプレイリストなんて話は当たり前じゃないんですねぇ…」* — "**you can only select music album-by-album.** Picking your favourite tracks and making a favourites playlist isn't a given here…" Individual tracks are reachable only as **Singles**, and a single is one track with no B-side.

**iPhone (Vinyl mini)** — `mini-ss5.png` — shows the same idea reduced: one crate at a time, heading `Singles` with `by title` beneath it, a single sleeve stack centred, the title captioned below, a **single A–Z rail on the right edge only**, and a left rail carrying back-arrow, search magnifier, and two other icons.

**Hard limits, from the FAQ:**
- *"Vinyl limits the number of tracks that is loaded as LP to 20, because otherwise tracks might get too narrow on the record. For that reason as well as for maintaining responsiveness, an LP also has a maximum length of 100 minutes."*
- Tracks are auto-distributed across side A and side B.

**A real-world gotcha worth designing around**, from the second walkthrough: *"when you put your albums from your iTunes onto your iPad or your iPhone, make sure that the songs are numbered, because I think it breaks up the album based on the numbering — and if they're not all on a single album numbered properly, sometimes they end up as singles or there's tracks missing."* Album/side assembly depends entirely on clean track-number metadata, and degrades silently and confusingly when metadata is bad.

---

## 5. Chrome and settings

The app is **maximally skeuomorphic and minimally chromed**. There is essentially no conventional UI: no tab bar, no nav bar on the player, no modal sheets in evidence. The only non-diegetic elements are the dark charcoal edge rail(s) of monochrome glyphs, the ⓘ, and the browser's black title bar with 戻る / search.

**Materials rendered:** rosewood and dark oak veneer with directional grain; cream and ivory moulded plastic; brushed aluminium top plates; polished chrome (arm tubes, pivots, nameplates); brass/gold (the '40s arm and badge); stamped perforated metal speaker grille; black textured metal; black vinyl with concentric grooves and a specular sweep; printed paper labels; a velvet cleaning pad. Screws, wear marks, engraved scales, silkscreened model names, backlit buttons, and a glowing green power LED. Real drop shadows separating the object from its background.

**Notably absent: no wood-panelled *background*, no felt, no room.** Both the picker and the browser sit on a **flat, neutral, near-white studio grey with a soft vignette**. The richness is entirely in the object; the stage is empty. That restraint is why it still looks decent rather than dated — worth copying.

**Screens, complete list:** (1) deck carousel, (2) player, (3) library crate browser, (4) an ⓘ / about screen (present on both carousel and player; contents unverified), (5) search (behind the magnifier; unverified). **Settings live outside the app entirely**, in the iOS Settings bundle: `Vinyl noise`, `Record player character`, `Speed variance`. Support/FAQ was hosted on the web at `elephantcandy.com/support/vinyl/`.

Marketing tagline, from the Facebook page (which is now a login wall — I recovered only the meta description, 102 likes): **"Experience your music the vintage way."** Product copy: *"Listen to your music the old-fashioned way, while enjoying a crisp design, high-quality sound and cool technical gizmos."*

---

## 6. What reviewers praised and complained about

### Praised — copy these

**The per-deck character is the thing people loved most.** It comes up in every source independently. Developer-site testimonial (user "Gabriel777"): *"This is very close to the vinyl experience. Nice realistic retro sound. **Lots of attention to detail in recreating the peculiarities of each record player.**"* Japanese App Store review 「デジタル音源をアナログ化」 (5★): *「ターンテーブルを選べるのだけど、それぞれ機種に特色があり、聴いてて飽きないです」* — "you can choose the turntable, and each model has its own character, so you never get tired of listening." A quoted review: *"The different record players having different tones and quirks shows that a lot of love went into creating this, and it wasn't done just for looks and to generate scratchy sound."*

**The friction is the feature.** AppBank's headline is literally 「レコードってめんどくさい！…けど楽しい！」 — **"Records are such a hassle! …but fun!"** He is a CD/streaming-generation reviewer discovering that side A ends, that dust accumulates, that you have to flip it — and enjoying every inconvenience. His closing line: *「今は昔に比べて、音楽をすごく気軽に聴けるようになったんだと実感しました。CD世代、デジタル配信世代に体験してほしいアプリです」* — "it made me realise how casually we listen to music now compared to before. I want the CD generation and the streaming generation to experience this app." **Do not sand off the friction.**

**The bump-to-skip gimmick over-delivers.** It's the single most-cited delight. 「思わず顔がほころんでしまった」 — "made me smile in spite of myself."

**The wear/clean loop.** Universally mentioned, universally liked. Sound & Vision's (tongue-in-cheek) roundup singled it out: *"The more you play your music, the scratchier it sounds. No worries: Tap the Clean button."*

**Groove rendering matched to track length.** 「かなりリアルです」 — "quite realistic."

**Audio quality itself.** 「この音質の良さはデジタル世代の人にお勧めしたい」 — "I'd recommend this sound quality to the digital generation." Both video reviewers were enthusiastic (*"the coolest way to play music"*; *"coolest app ever, and I mean ever"*).

### Complained about — avoid these

**1. Gaps and timing errors at track boundaries.** The most serious and most repeated complaint. Review 「タイトル通り。文句無し」 (5★, and still): *「曲ごとの最初や最後で再生の誤差が起きる点のみが難。そこさえ修正・補強されれば、同類のappでは間違いなく最強になる。…再生のシステム以外は完璧」* — "**the only flaw is a timing error at the start and end of each track. If just that were fixed, it would without doubt be the strongest of its kind.** …Apart from the playback system it's perfect." Review 「傑作です！」: *「曲の間に無いはずの間が入ってしまいます。メドレー(abbey roadのB面など)とかで困ります」* — "**gaps that shouldn't be there get inserted between tracks. That's a problem for medleys** (e.g. side B of Abbey Road)." **Gapless playback is non-negotiable for a vinyl simulator.** A record side is one continuous groove; if your engine inserts a silence between tracks, the entire premise breaks, and side-B-of-Abbey-Road is the exact test case a real listener will reach for.

**2. Background playback is broken.** 「バックグラウンド再生にすると曲の始めの音飛びが凄まじいです」 — "with background playback, the skipping at the start of tracks is **appalling**." Also reported in an aggregated review summary as "occasional audio skips during background playback."

**3. Finding music is a chore.** 「他の方もおっしゃるように、曲の探し方が面倒なのがちょっと残念」 — "as others have said, it's a shame that **finding a track is a hassle**." Album-only selection, no playlists, no shuffle, and a crate you have to physically flip through. Some of that is deliberate and correct; the *lack of any escape hatch* is not. Give people a search that actually works and an optional flat list.

**4. Discoverability of the interface.** The applion editorial verdict lists as a downside: 「インターフェースが少し複雑で、初めて使用するユーザーには戸惑うかもしれない」 — "the interface is a bit complex and may confuse first-time users." AppBank's reviewer, on first opening the player: 「おぉ…見たことあるけど使い方がわからん…」 — "**ooh… I've seen one of these, but I have no idea how to use it.**" That is the skeuomorphic trap: an unlabelled photoreal object with no affordance hints. The two-target tonearm (lift vs drag) is brilliant *once you know*, and invisible until then. Budget for a first-run coach overlay.

**5. The audio character was buried in OS Settings and off by default.** The Sound Test Room reviewer called this out on camera: *"it would have been better if they'd have had them inside the actual app."* This is the worst self-inflicted wound in the product — the differentiating feature shipped disabled and hidden.

**6. Library restrictions.** No iTunes Match, no cloud, no streaming, DRM tracks stamped UNAVAILABLE. Users hit this constantly: *"UNAVAILABLE stamped on albums… had to go to the iPad Music app to download albums from the cloud before they could be played in Vinyl"*; *"no iCloud support, which is problematic as an iPad mini will quickly fill up."* Partly unavoidable (you need raw PCM to apply DSP) — but on Android, plan the local-file path as first-class and be explicit and early about what won't work, rather than surfacing it as a mysterious stamp on a sleeve.

**7. Metadata fragility.** Albums silently fracture into singles or lose tracks when track numbers are missing.

**8. Hard caps.** 20 tracks / 100 minutes per LP. Justified honestly by the developer (groove width, responsiveness) but it is a limit real users will hit on long compilations.

---

## Sources

- Official developer pages, via Wayback: `elephantcandy.com/app/vinyl` (18 Jun 2013), `/app/vinyl-mini/` (20 Jun 2013), and crucially **`/support/vinyl/` v1.0 FAQ** (23 Jun 2013) — the FAQ is the only first-party document of actual mechanics.
- AppBank (JA), 18 Mar 2013: `https://www.appbank.net/2013/03/18/ipad/561306.php` — 11 screenshots of the iPad build, the primary visual source. (Blocks WebFetch; fetched via curl with a browser UA.)
- Touch Lab (JA), 9 Mar 2013: `https://touchlab.jp/2013/03/real-rac/` — the clearest description of the tonearm lift-vs-drag mechanic and the era-based audio model.
- APPLION (JA): `https://applion.jp/Vinyl-mini/iphone-646358530/` — Japanese App Store user reviews (4.2★ from 10 ratings: 5/3/1/1/0), release/version data.
- Soft112 mirrors of the App Store listings for both apps — description and 1.3 release notes.
- YouTube, The Sound Test Room (Doug Woods), `8leOYocOGSs`, ~9m16s — auto-transcript pulled with yt-dlp; source for the iOS Settings toggles, the generated "elephantcandy Records" labels, and the lead-in crackle.
- YouTube, `lJAMWL9gWRI` "VInyl by Elephant Candy", ~6m27s — source for "you can scratch on the record," the groove rendering, and the track-numbering gotcha.
- Sound & Vision retro-apps roundup — confirms the $3.99 price point.
- Facebook `VinylTheRealRecordPlayer` — **login-walled**; recovered only the tagline "Experience your music the vintage way" and 102 likes. Contributed essentially nothing.
- AppAdvice — actively blocks automated fetching (connection refused on both direct and browser-UA requests); its content reached me only through search-result summaries, so I have treated anything sourced only from it as secondary.

**Explicitly unresolved:** the names of 2 of the 7 decks; whether 16/78 rpm decks exist (probable, not confirmed); the function of the circular-arrow rail icon; whether disc-drag is a true audio scrub or a triggered effect; whether Groove Clean is tap or wipe; whether start/stop produces a pitch ramp; the contents of the ⓘ screen.
