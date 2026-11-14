// Page transitions. Adapters run leave() while the next page loads, so a transition adds
// little or no latency. The site's own look is registered with setTransition() in motion/setup.
import { gsap } from 'gsap';
import { ease, prefersReducedMotion } from './env';

export type Transition = {
  leave: (curtain: HTMLElement) => Promise<unknown> | gsap.core.Animation;
  enter: (curtain: HTMLElement) => Promise<unknown> | gsap.core.Animation;
};

let current: Transition = {
  leave: (c) => gsap.timeline()
    .set(c, { autoAlpha: 1 })
    .fromTo(c.querySelector('.curtain__panel'), { xPercent: 100 }, { xPercent: 0, duration: 0.8, ease: ease.inOut }),
  enter: (c) => gsap.timeline()
    .to(c.querySelector('.curtain__panel'), { xPercent: -100, duration: 0.9, ease: ease.inOut })
    .set(c, { autoAlpha: 0 }),
};

const reduced: Transition = {
  leave: (c) => gsap.timeline()
    .set(c, { autoAlpha: 1 })
    .set(c.querySelector('.curtain__panel'), { xPercent: 0 })
    .fromTo(c, { opacity: 0 }, { opacity: 1, duration: 0.2 }),
  enter: (c) => gsap.to(c, { autoAlpha: 0, duration: 0.2 }),
};

export const setTransition = (t: Transition) => { current = t; };
const pick = () => (prefersReducedMotion() ? reduced : current);
const curtain = () => document.getElementById('curtain') as HTMLElement;

export const playLeave = () => Promise.resolve(pick().leave(curtain()));
export const playEnter = () => Promise.resolve(pick().enter(curtain()));
