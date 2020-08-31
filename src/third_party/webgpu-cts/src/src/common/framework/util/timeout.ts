declare const step_timeout: undefined | typeof setTimeout;

export const timeout = typeof step_timeout !== 'undefined' ? step_timeout : setTimeout;
