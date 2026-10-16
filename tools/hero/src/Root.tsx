import { Composition } from "remotion";
import { HERO_SECONDS, HeroStage } from "./HeroStage";

export const RemotionRoot: React.FC = () => {
  return (
    <>
      <Composition
        id="hero-en"
        component={HeroStage}
        durationInFrames={Math.round(HERO_SECONDS * 25)}
        fps={25}
        width={1280}
        height={640}
        defaultProps={{ lang: "en" as const }}
      />
      <Composition
        id="hero-ru"
        component={HeroStage}
        durationInFrames={Math.round(HERO_SECONDS * 25)}
        fps={25}
        width={1280}
        height={640}
        defaultProps={{ lang: "ru" as const }}
      />
    </>
  );
};
