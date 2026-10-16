import type React from "react";
import { createContext, useContext } from "react";
import {
  AbsoluteFill,
  Easing,
  Img,
  interpolate,
  Sequence,
  staticFile,
  useCurrentFrame,
  useVideoConfig,
} from "remotion";

// The README hero, told in order and always in the middle of the frame:
// the promise, the shortcut being pressed, the picker, a pick, and "you're signed in".
// It starts and ends on an empty stage, so the GIF loops without a jump.

const clamp = { extrapolateLeft: "clamp", extrapolateRight: "clamp" } as const;
const out = Easing.bezier(0.16, 1, 0.3, 1);
const inOut = Easing.bezier(0.65, 0, 0.35, 1);
const away = Easing.bezier(0.5, 0, 0.75, 0);
const text = '"Segoe UI Variable Text", "Segoe UI", sans-serif';
const display = '"Segoe UI Variable Display", "Segoe UI", sans-serif';
const accent = "#f08a62";
const green = "#4fc06a";

// Scene starts, in seconds. Neighbours overlap so one hands over to the next.
const T = { keys: 1.9, picker: 3.15, done: 5.05, end: 9.0 };
export const HERO_SECONDS = T.end;

// The picker capture at 1x, centred, and the Work card inside it.
const PICKER = { w: 612, h: 334, bar: 32, x: (1280 - 612) / 2, y: 196 };
const CARDS = [24, 168, 312, 456].map((x) => ({ x, y: 58, w: 132, h: 204 }));
const WORK_BOX = {
  x: PICKER.x + CARDS[1].x,
  y: PICKER.y + PICKER.bar + CARDS[1].y,
  w: CARDS[1].w,
  h: CARDS[1].h,
};
const DONE_BOX = { x: 440, y: 236, w: 400, h: 240 };

export type Lang = "en" | "ru";
type Copy = {
  pill: string;
  title: [string[], string[]];
  window: string;
  names: [string, string, string];
  signedIn: string;
  keys: [string, string];
  pick: string;
  done: [string, string];
};

// Account names match the demo profiles in the captures (tools/demo.ps1).
const COPY: Record<Lang, Copy> = {
  en: {
    pill: "for Windows",
    title: [
      ["Every", "Claude", "account."],
      ["One", "shortcut."],
    ],
    window: "Claude Accounts",
    names: ["Main", "Work", "Personal"],
    signedIn: "Signed in",
    keys: ["Press", "in any app"],
    pick: "Pick an account",
    done: ["You're in.", "Every account stays signed in, side by side."],
  },
  ru: {
    pill: "для Windows",
    title: [
      ["Все", "аккаунты", "Claude."],
      ["Одно", "сочетание", "клавиш."],
    ],
    window: "Аккаунты Claude",
    names: ["Основной", "Работа", "Личный"],
    signedIn: "Вход выполнен",
    keys: ["Нажмите", "в любом приложении"],
    pick: "Выберите аккаунт",
    done: [
      "Готово.",
      "Все аккаунты открыты сразу, и выходить ни из одного не нужно.",
    ],
  },
};

const LangContext = createContext<Lang>("en");
const useCopy = () => COPY[useContext(LangContext)];
// tools/hero/render.ps1 copies each language's window captures and icons into public/<lang>.
const useAsset = () => {
  const lang = useContext(LangContext);
  return (path: string) => staticFile(`${lang}/${path}`);
};

// Local time of the enclosing <Sequence>, in seconds, and a clamped tween over it.
const useSeconds = () => {
  const frame = useCurrentFrame();
  const { fps } = useVideoConfig();
  return frame / fps;
};
const tw = (
  t: number,
  at: number[],
  to: number[],
  easing: (x: number) => number = out,
) => interpolate(t, at, to, { ...clamp, easing });

// Kept still on purpose: a moving full-frame glow makes every GIF frame differ everywhere.
const Background: React.FC = () => (
  <AbsoluteFill style={{ background: "#09080a" }}>
    <div
      style={{
        position: "absolute",
        left: -10,
        top: -360,
        width: 1300,
        height: 860,
        background:
          "radial-gradient(closest-side, rgba(226,118,78,.40), rgba(226,118,78,.10) 55%, transparent 100%)",
      }}
    />
    <AbsoluteFill
      style={{
        backgroundImage:
          "radial-gradient(rgba(255,255,255,.11) 1px, transparent 1.3px)",
        backgroundSize: "24px 24px",
        WebkitMaskImage:
          "radial-gradient(720px 400px at 50% 60%, #000 0%, transparent 75%)",
      }}
    />
    <AbsoluteFill
      style={{
        background:
          "radial-gradient(1100px 640px at 50% 45%, transparent 55%, rgba(0,0,0,.55) 100%)",
      }}
    />
  </AbsoluteFill>
);

// The promise. Big in the middle first, then it rises and stays as the header.
const Title: React.FC = () => {
  const t = useSeconds();
  const copy = useCopy();
  const asset = useAsset();
  // Each word carries its own gradient: background-clip text does not reach into children that blur.
  const word = (w: string, i: number, line: number) => {
    const at = 0.15 + line * 0.38 + i * 0.07;
    const warm: React.CSSProperties =
      line === 1
        ? {
            background:
              "linear-gradient(92deg, #f6ac88 0%, #e0764f 38%, #ffd9c6 50%, #e0764f 62%, #c95f3c 100%)",
            backgroundSize: "300% 100%",
            backgroundPositionX: `${tw(t, [0.9 + i * 0.1, 2.0 + i * 0.1], [100, 0], inOut)}%`,
            WebkitBackgroundClip: "text",
            color: "transparent",
          }
        : {};
    return (
      <span
        key={w}
        style={{
          display: "inline-block",
          marginRight: "0.24em",
          opacity: tw(t, [at, at + 0.35], [0, 1]),
          translate: `0px ${tw(t, [at, at + 0.5], [26, 0])}px`,
          filter: `blur(${tw(t, [at, at + 0.4], [8, 0])}px)`,
          ...warm,
        }}
      >
        {w}
      </span>
    );
  };
  const up = tw(t, [1.55, 2.2], [0, 1], inOut);
  return (
    <AbsoluteFill
      style={{
        alignItems: "center",
        justifyContent: "center",
        opacity: tw(t, [T.end - 0.55, T.end - 0.2], [1, 0], away),
      }}
    >
      <div
        style={{
          display: "flex",
          flexDirection: "column",
          alignItems: "center",
          translate: `0px ${-236 * up}px`,
          scale: 1 - 0.58 * up,
        }}
      >
        <div
          style={{
            display: "inline-flex",
            alignItems: "center",
            gap: 9,
            marginBottom: 26,
            padding: "6px 15px 6px 7px",
            borderRadius: 999,
            font: `600 14px ${text}`,
            color: "rgba(255,255,255,.72)",
            background: "rgba(255,255,255,.05)",
            border: "1px solid rgba(255,255,255,.10)",
            opacity: tw(t, [0.05, 0.4], [0, 1]) * (1 - up),
          }}
        >
          <Img
            src={asset("ico/app-64.png")}
            style={{ width: 22, height: 22 }}
          />
          Claude Profile Switcher
          <i
            style={{
              width: 1,
              height: 13,
              background: "rgba(255,255,255,.18)",
            }}
          />
          {copy.pill}
        </div>
        <div
          style={{
            font: `700 84px/1.0 ${display}`,
            letterSpacing: -3.4,
            textAlign: "center",
            whiteSpace: "nowrap",
          }}
        >
          <div>{copy.title[0].map((w, i) => word(w, i, 0))}</div>
          <div>{copy.title[1].map((w, i) => word(w, i, 1))}</div>
        </div>
      </div>
    </AbsoluteFill>
  );
};

// One line under the action that says what is happening.
const Caption: React.FC<{ until: number; children: React.ReactNode }> = ({
  until,
  children,
}) => {
  const t = useSeconds();
  return (
    <AbsoluteFill style={{ top: 572, height: 40, alignItems: "center" }}>
      <div
        style={{
          font: `500 19px ${text}`,
          color: "rgba(255,255,255,.66)",
          opacity: tw(t, [0, 0.3, until - 0.3, until], [0, 1, 1, 0], inOut),
          translate: `0px ${tw(t, [0, 0.4], [10, 0])}px`,
        }}
      >
        {children}
      </div>
    </AbsoluteFill>
  );
};
const Hi: React.FC<{ children: React.ReactNode; color?: string }> = ({
  children,
  color = "#fff",
}) => <b style={{ color, fontWeight: 600 }}>{children}</b>;

const Keycap: React.FC<{
  label: string;
  wide?: boolean;
  appear: number;
  down: number;
  up: number;
}> = ({ label, wide, appear, down, up }) => {
  const t = useSeconds();
  const p = tw(t, [down, down + 0.08, up, up + 0.12], [0, 1, 1, 0], inOut);
  const edge = 7 - 6 * p;
  return (
    <div
      style={{
        width: wide ? 150 : 118,
        height: 112,
        borderRadius: 18,
        display: "grid",
        placeItems: "center",
        font: `600 36px ${display}`,
        color: p > 0.5 ? "#ffb391" : "#f3f3f3",
        background:
          p > 0.5
            ? "linear-gradient(#3a2a25, #241a17)"
            : "linear-gradient(#302f33, #1c1b1e)",
        opacity: tw(t, [appear, appear + 0.3], [0, 1]),
        scale: tw(t, [appear, appear + 0.45], [0.82, 1]),
        translate: `0px ${6 * p}px`,
        boxShadow: [
          `inset 0 1px 0 rgba(255,255,255,${0.16 - 0.1 * p})`,
          `0 0 0 1px ${p > 0.5 ? "rgba(240,138,98,.9)" : "rgba(255,255,255,.09)"}`,
          `0 ${edge}px 0 #0b0a0c`,
          `0 ${22 - 12 * p}px 34px rgba(0,0,0,.6)`,
          `0 0 ${46 * p}px rgba(240,138,98,${0.55 * p})`,
        ].join(", "),
      }}
    >
      {label}
    </div>
  );
};

// Ctrl, Alt, C go down one after another; on release a ring of light opens the picker.
const KeysScene: React.FC = () => {
  const t = useSeconds();
  const release = 1.12;
  const plus = { font: `500 34px ${display}`, color: "rgba(255,255,255,.28)" };
  return (
    <AbsoluteFill style={{ alignItems: "center", justifyContent: "center" }}>
      <div
        style={{
          position: "absolute",
          width: 520,
          height: 120,
          top: 380,
          borderRadius: "50%",
          background:
            "radial-gradient(closest-side, rgba(240,138,98,.45), transparent)",
          filter: "blur(10px)",
          opacity: tw(
            t,
            [0.55, 1.0, release, release + 0.4],
            [0, 0.9, 1, 0],
            inOut,
          ),
        }}
      />
      <div
        style={{
          display: "flex",
          alignItems: "center",
          gap: 18,
          translate: `0px ${-10 + tw(t, [release + 0.05, release + 0.5], [0, -40], away)}px`,
          scale: tw(t, [release + 0.05, release + 0.5], [1, 0.7], away),
          opacity: tw(t, [release + 0.1, release + 0.45], [1, 0], away),
          filter: `blur(${tw(t, [release + 0.1, release + 0.45], [0, 6], away)}px)`,
        }}
      >
        <Keycap label="Ctrl" wide appear={0.05} down={0.5} up={release} />
        <span style={plus}>+</span>
        <Keycap label="Alt" appear={0.12} down={0.72} up={release} />
        <span style={plus}>+</span>
        <Keycap label="C" appear={0.19} down={0.94} up={release} />
      </div>
      <div
        style={{
          position: "absolute",
          left: 640 - 60,
          top: 310 - 60,
          width: 120,
          height: 120,
          borderRadius: "50%",
          border: `2px solid ${accent}`,
          boxShadow: `0 0 30px rgba(240,138,98,.6), inset 0 0 30px rgba(240,138,98,.4)`,
          opacity: tw(
            t,
            [release - 0.01, release, release + 0.5],
            [0, 0.7, 0],
            Easing.linear,
          ),
          scale: tw(t, [release, release + 0.5], [0.3, 4.2]),
        }}
      />
    </AbsoluteFill>
  );
};

// One rectangle of the picker capture, drawn where it sits in the window.
const Crop: React.FC<{
  box: { x: number; y: number; w: number; h: number };
  style?: React.CSSProperties;
}> = ({ box, style }) => {
  const asset = useAsset();
  return (
    <div
      style={{
        position: "absolute",
        left: box.x,
        top: box.y,
        width: box.w,
        height: box.h,
        overflow: "hidden",
        ...style,
      }}
    >
      <Img
        src={asset("picker-dark.png")}
        style={{
          position: "absolute",
          left: -box.x,
          top: -box.y,
          width: PICKER.w,
          height: PICKER.h,
        }}
      />
    </div>
  );
};

const Cursor: React.FC<{
  path: [number, number, number][];
  click: number;
  show: [number, number];
}> = ({ path, click, show }) => {
  const t = useSeconds();
  const times = path.map((p) => p[0]);
  const x = tw(
    t,
    times,
    path.map((p) => p[1]),
    inOut,
  );
  const y = tw(
    t,
    times,
    path.map((p) => p[2]),
    inOut,
  );
  return (
    <>
      <div
        style={{
          position: "absolute",
          left: x - 24,
          top: y - 24,
          width: 48,
          height: 48,
          borderRadius: "50%",
          border: "2px solid rgba(255,240,230,.9)",
          opacity: tw(
            t,
            [click - 0.01, click, click + 0.5],
            [0, 0.9, 0],
            Easing.linear,
          ),
          scale: tw(t, [click, click + 0.5], [0.3, 1.4]),
        }}
      />
      <svg
        viewBox="0 0 26 34"
        width={26}
        height={34}
        style={{
          position: "absolute",
          left: x - 2,
          top: y - 2,
          transformOrigin: "2px 2px",
          filter: "drop-shadow(0 3px 6px rgba(0,0,0,.45))",
          opacity: tw(
            t,
            [show[0], show[0] + 0.2, show[1] - 0.2, show[1]],
            [0, 1, 1, 0],
            Easing.linear,
          ),
          scale: tw(
            t,
            [click, click + 0.05, click + 0.17],
            [1, 0.82, 1],
            Easing.linear,
          ),
        }}
      >
        <path
          d="M2 2 L2 27 L8.2 21.4 L12.6 31.2 L17 29.2 L12.7 19.6 L21 19.6 Z"
          fill="#fff"
          stroke="#111"
          strokeWidth={1.6}
          strokeLinejoin="round"
        />
      </svg>
    </>
  );
};

// The picker grows out of the light, the cards land one by one, the cursor picks Work.
const PickerScene: React.FC = () => {
  const t = useSeconds();
  const copy = useCopy();
  const asset = useAsset();
  const click = 1.62;
  const leave = 1.9;
  const work = { x: WORK_BOX.x + 72, y: WORK_BOX.y + 58 };
  return (
    <AbsoluteFill>
      <div
        style={{
          position: "absolute",
          left: PICKER.x,
          top: PICKER.y,
          width: PICKER.w,
          height: PICKER.h + PICKER.bar,
          borderRadius: 10,
          overflow: "hidden",
          background: "#202020",
          boxShadow:
            "0 0 0 1px rgba(255,255,255,.10), 0 -1px 0 rgba(255,190,160,.30), 0 50px 100px -20px rgba(0,0,0,.85), 0 0 160px rgba(226,118,78,.22)",
          opacity:
            tw(t, [0, 0.25], [0, 1]) *
            tw(t, [leave, leave + 0.4], [1, 0], away),
          scale:
            tw(t, [0, 0.6], [0.86, 1]) *
            tw(t, [leave, leave + 0.45], [1, 0.96], away),
          translate: `0px ${tw(t, [0, 0.6], [34, 0])}px`,
          filter: `blur(${tw(t, [leave, leave + 0.4], [0, 5], away)}px)`,
        }}
      >
        <div
          style={{
            height: PICKER.bar,
            display: "flex",
            alignItems: "center",
            gap: 10,
            paddingLeft: 12,
            fontSize: 12,
            color: "rgba(255,255,255,.86)",
          }}
        >
          <Img
            src={asset("ico/app-32.png")}
            style={{ width: 16, height: 16 }}
          />
          {copy.window}
          <i
            style={{
              marginLeft: "auto",
              width: 46,
              display: "grid",
              placeItems: "center",
              fontFamily: '"Segoe Fluent Icons", "Segoe MDL2 Assets"',
              fontSize: 10,
              fontStyle: "normal",
            }}
          >
            {""}
          </i>
        </div>
        <div
          style={{ position: "relative", width: PICKER.w, height: PICKER.h }}
        >
          <Crop
            box={{ x: 0, y: 0, w: 612, h: 56 }}
            style={{ opacity: tw(t, [0.15, 0.45], [0, 1]) }}
          />
          <Crop
            box={{ x: 0, y: 266, w: 612, h: 68 }}
            style={{ opacity: tw(t, [0.55, 0.85], [0, 1]) }}
          />
          {CARDS.map((box, i) => (
            <Crop
              key={i}
              box={box}
              style={{
                opacity:
                  tw(t, [0.22 + i * 0.07, 0.5 + i * 0.07], [0, 1]) *
                  (i === 1
                    ? 1
                    : tw(t, [click + 0.1, click + 0.35], [1, 0.35], inOut)),
                translate: `0px ${tw(t, [0.22 + i * 0.07, 0.7 + i * 0.07], [26, 0])}px`,
                scale:
                  i === 1
                    ? tw(
                        t,
                        [click, click + 0.06, click + 0.2],
                        [1, 0.965, 1],
                        inOut,
                      )
                    : 1,
              }}
            />
          ))}
          <div
            style={{
              position: "absolute",
              left: CARDS[1].x - 2,
              top: CARDS[1].y - 2,
              width: CARDS[1].w + 4,
              height: CARDS[1].h + 4,
              borderRadius: 8,
              boxShadow: `0 0 0 2px ${accent}, 0 0 26px rgba(240,138,98,.55)`,
              opacity: tw(t, [1.28, 1.42], [0, 1], Easing.linear),
            }}
          />
        </div>
      </div>
      <Cursor
        click={click}
        show={[0.85, leave + 0.25]}
        path={[
          [0.85, 1010, 610],
          [1.5, work.x, work.y],
          [leave + 0.25, work.x, work.y],
        ]}
      />
    </AbsoluteFill>
  );
};

const Check: React.FC<{ at: number; size: number }> = ({ at, size }) => {
  const t = useSeconds();
  return (
    <svg
      width={size}
      height={size}
      viewBox="0 0 24 24"
      style={{
        flex: "none",
        scale: tw(t, [at, at + 0.35], [0.4, 1]),
        opacity: tw(t, [at, at + 0.15], [0, 1]),
      }}
    >
      <circle cx={12} cy={12} r={11} fill={green} />
      <path
        d="M7 12.4 L10.4 15.6 L17 8.8"
        fill="none"
        stroke="#fff"
        strokeWidth={2.4}
        strokeLinecap="round"
        strokeLinejoin="round"
        strokeDasharray={16}
        strokeDashoffset={tw(t, [at + 0.12, at + 0.45], [16, 0], inOut)}
      />
    </svg>
  );
};

const Account: React.FC<{ index: 0 | 1 | 2; at: number; big?: boolean }> = ({
  index,
  at,
  big,
}) => {
  const copy = useCopy();
  const asset = useAsset();
  return (
    <div
      style={{
        display: "flex",
        flexDirection: "column",
        alignItems: "center",
        gap: big ? 14 : 10,
      }}
    >
      <div
        style={{
          position: "relative",
          width: big ? 92 : 60,
          height: big ? 92 : 60,
        }}
      >
        <Img
          src={asset(`ico/account-${index + 1}.png`)}
          style={{ width: "100%", height: "100%" }}
        />
      </div>
      <div
        style={{
          font: `700 ${big ? 30 : 20}px ${display}`,
          letterSpacing: big ? -0.6 : -0.2,
        }}
      >
        {copy.names[index]}
      </div>
      <div
        style={{
          display: "flex",
          alignItems: "center",
          gap: 8,
          font: `600 ${big ? 17 : 13.5}px ${text}`,
          color: green,
        }}
      >
        <Check at={at} size={big ? 22 : 16} />
        {copy.signedIn}
      </div>
    </div>
  );
};

// The picked card grows into "Work, signed in", and the other accounts join it, signed in too.
const DoneScene: React.FC = () => {
  const t = useSeconds();
  const grow = tw(t, [0, 0.6], [0, 1]);
  const lerp = (a: number, b: number) => a + (b - a) * grow;
  const fade = tw(
    t,
    [T.end - T.done - 0.6, T.end - T.done - 0.2],
    [1, 0],
    away,
  );
  const side = (dir: number, at: number): React.CSSProperties => ({
    position: "absolute",
    top: 268,
    left: dir < 0 ? 130 : 890,
    width: 260,
    height: 176,
    borderRadius: 16,
    display: "grid",
    placeItems: "center",
    background:
      "linear-gradient(180deg, rgba(44,42,46,.92), rgba(30,29,32,.92))",
    boxShadow:
      "0 0 0 1px rgba(255,255,255,.08), 0 30px 60px -20px rgba(0,0,0,.8)",
    opacity: tw(t, [at, at + 0.35], [0, 0.88]) * fade,
    translate: `${tw(t, [at, at + 0.6], [-dir * 90, 0])}px 0px`,
    scale: tw(t, [at, at + 0.6], [0.9, 1]),
  });
  return (
    <AbsoluteFill>
      <div style={side(-1, 1.45)}>
        <Account index={0} at={1.75} />
      </div>
      <div style={side(1, 1.55)}>
        <Account index={2} at={1.85} />
      </div>
      <div
        style={{
          position: "absolute",
          left: lerp(WORK_BOX.x, DONE_BOX.x),
          top: lerp(WORK_BOX.y, DONE_BOX.y),
          width: lerp(WORK_BOX.w, DONE_BOX.w),
          height: lerp(WORK_BOX.h, DONE_BOX.h),
          borderRadius: lerp(6, 20),
          overflow: "hidden",
          background: "linear-gradient(180deg, #2f2c2e, #211f22)",
          boxShadow: `0 0 0 1px rgba(255,255,255,.10), 0 0 0 ${2 * (1 - grow)}px ${accent}, 0 50px 100px -24px rgba(0,0,0,.9), 0 0 120px rgba(240,138,98,${0.12 + 0.1 * grow})`,
          opacity: fade,
          scale: tw(
            t,
            [T.end - T.done - 0.6, T.end - T.done - 0.2],
            [1, 0.96],
            away,
          ),
        }}
      >
        <Crop
          box={CARDS[1]}
          style={{
            left: 0,
            top: 0,
            opacity: tw(t, [0.05, 0.25], [1, 0], Easing.linear),
          }}
        />
        <div
          style={{
            position: "absolute",
            inset: 0,
            display: "grid",
            placeItems: "center",
            opacity: tw(t, [0.35, 0.65], [0, 1]),
            translate: `0px ${tw(t, [0.35, 0.8], [12, 0])}px`,
          }}
        >
          <Account index={1} at={0.7} big />
        </div>
      </div>
    </AbsoluteFill>
  );
};

export const HeroStage: React.FC<{ lang: Lang }> = ({ lang }) => {
  const { fps } = useVideoConfig();
  const f = (seconds: number) => Math.round(seconds * fps);
  const copy = COPY[lang];
  return (
    <LangContext.Provider value={lang}>
      <AbsoluteFill
        style={{ fontFamily: text, color: "#fff", overflow: "hidden" }}
      >
        <Background />
        <Sequence name="Title" durationInFrames={f(T.end)}>
          <Title />
        </Sequence>
        <Sequence
          name="Keys"
          from={f(T.keys)}
          durationInFrames={f(T.picker - T.keys + 0.6)}
        >
          <KeysScene />
        </Sequence>
        <Sequence
          name="Caption keys"
          from={f(T.keys + 0.2)}
          durationInFrames={f(T.picker - T.keys)}
        >
          <Caption until={T.picker - T.keys}>
            {copy.keys[0]} <Hi>Ctrl + Alt + C</Hi> {copy.keys[1]}
          </Caption>
        </Sequence>
        <Sequence
          name="Picker"
          from={f(T.picker)}
          durationInFrames={f(T.done - T.picker + 0.6)}
        >
          <PickerScene />
        </Sequence>
        <Sequence
          name="Caption picker"
          from={f(T.picker + 0.2)}
          durationInFrames={f(T.done - T.picker)}
        >
          <Caption until={T.done - T.picker}>{copy.pick}</Caption>
        </Sequence>
        <Sequence
          name="Done"
          from={f(T.done)}
          durationInFrames={f(T.end - T.done)}
        >
          <DoneScene />
        </Sequence>
        <Sequence
          name="Caption done"
          from={f(T.done + 0.5)}
          durationInFrames={f(T.end - T.done - 0.7)}
        >
          <Caption until={T.end - T.done - 0.7}>
            <Hi color={green}>{copy.done[0]}</Hi> {copy.done[1]}
          </Caption>
        </Sequence>
      </AbsoluteFill>
    </LangContext.Provider>
  );
};
