/** An item in a trade, as sent by the client. Short keys keep the JSON small. */
export interface TradeItem {
  /** Index in the local player's tradable items (only meaningful for the local inventory). */
  i: number;
  /** Name. */
  n: string;
  /** Count. */
  c: number;
  /** Gold value of one, -1 when unknown. */
  v: number;
  /** Weight of one. */
  w: number;
  /** Category: weapons, apparel, potions, ingredients, scrolls, books, keys, gold, misc. */
  t: string;
  /** Enchanted. */
  e: boolean;
  /** Poisoned. */
  p: boolean;
}

/** Matches TradeCloseReason in Code/encoding/Structs/TradeTypes.h. */
export enum TradeCloseReason {
  Completed = 0,
  Cancelled,
  Declined,
  Busy,
  Disconnected,
  Failed,
  Expired,
}

export interface TradeSession {
  partnerId: number;
  partnerName: string;
  /** The local player's tradable items. */
  inventory: TradeItem[];
  /** How many of each inventory item (by index) the local player offers. */
  offer: Record<number, number>;
  partnerOffer: TradeItem[];
  selfReady: boolean;
  partnerReady: boolean;
  /** Both accepted; the items are changing hands. */
  handingOver: boolean;
}

export const TRADE_CATEGORIES = [
  'all',
  'weapons',
  'apparel',
  'potions',
  'ingredients',
  'books',
  'misc',
] as const;

export type TradeCategory = (typeof TRADE_CATEGORIES)[number];

export function tradeCategoryOf(item: TradeItem): TradeCategory {
  switch (item.t) {
    case 'weapons':
    case 'apparel':
    case 'potions':
    case 'ingredients':
    case 'books':
      return item.t;
    case 'scrolls':
      return 'books';
    default:
      return 'misc';
  }
}
