// The utility's own interface screenshots, per language. Russian falls back to English where
// no Russian capture exists.
import type { ImageMetadata } from 'astro';
import type { Lang, Shot } from './site';

const files = import.meta.glob<{ default: ImageMetadata }>('../assets/shots/*/*.png', { eager: true });

export function shot(lang: Lang, name: Shot): ImageMetadata {
  const pick = (l: Lang) => files[`../assets/shots/${l}/${name}.png`]?.default;
  const img = pick(lang) ?? pick('en');
  if (!img) throw new Error(`missing screenshot ${name}`);
  return img;
}
