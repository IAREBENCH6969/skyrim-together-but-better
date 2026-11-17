import {
  ChangeDetectionStrategy,
  Component,
  EventEmitter,
  HostListener,
  Output,
} from '@angular/core';
import {
  faSkullCrossbones,
  faWandMagicSparkles,
} from '@fortawesome/free-solid-svg-icons';
import { BehaviorSubject, combineLatest, map } from 'rxjs';
import {
  TRADE_CATEGORIES,
  TradeCategory,
  TradeItem,
  TradeSession,
  tradeCategoryOf,
} from '../../models/trade';
import { Sound, SoundService } from '../../services/sound.service';
import { TradeService } from '../../services/trade.service';

interface InventoryRow {
  item: TradeItem;
  offered: number;
  available: number;
}

interface OfferRow {
  item: TradeItem;
  count: number;
}

interface TradeView {
  session: TradeSession;
  rows: InventoryRow[];
  offer: OfferRow[];
  offerValue: number;
  offerWeight: number;
  partnerValue: number;
  partnerWeight: number;
}

const byName = (a: TradeItem, b: TradeItem) =>
  a.t === 'gold' ? -1 : b.t === 'gold' ? 1 : a.n.localeCompare(b.n);

function totals(rows: { item: TradeItem; count: number }[]) {
  let value = 0;
  let weight = 0;
  for (const { item, count } of rows) {
    if (item.t === 'gold') {
      value += count;
    } else if (item.v > 0) {
      value += item.v * count;
    }
    weight += item.w * count;
  }
  return { value, weight };
}

/** The trade window: your inventory on the left, both offers on the right, like the game's barter menu. */
@Component({
  selector: 'app-trade',
  templateUrl: './trade.component.html',
  styleUrls: ['./trade.component.scss'],
  changeDetection: ChangeDetectionStrategy.OnPush,
  standalone: false,
})
export class TradeComponent {
  @Output() public done = new EventEmitter<void>();

  readonly categories = TRADE_CATEGORIES;
  readonly enchantedIcon = faWandMagicSparkles;
  readonly poisonedIcon = faSkullCrossbones;
  readonly category$ = new BehaviorSubject<TradeCategory>('all');
  readonly search$ = new BehaviorSubject<string>('');

  /** The item whose quantity is being chosen ("How many?"), and the amount. */
  picking: TradeItem | null = null;
  pickCount = 1;

  readonly view$ = combineLatest([
    this.trade.session,
    this.category$,
    this.search$,
  ]).pipe(
    map(([session, category, search]): TradeView | null => {
      if (!session) {
        return null;
      }
      const needle = search.trim().toLowerCase();
      const rows = session.inventory
        .filter(
          item =>
            (category === 'all' || tradeCategoryOf(item) === category) &&
            (!needle || item.n.toLowerCase().includes(needle)),
        )
        .sort(byName)
        .map(item => {
          const offered = session.offer[item.i] ?? 0;
          return { item, offered, available: item.c - offered };
        });
      const offer = session.inventory
        .filter(item => session.offer[item.i])
        .sort(byName)
        .map(item => ({ item, count: session.offer[item.i] }));
      const partner = session.partnerOffer
        .slice()
        .sort(byName)
        .map(item => ({ item, count: item.c }));
      const mine = totals(offer);
      const theirs = totals(partner);
      return {
        session,
        rows,
        offer,
        offerValue: mine.value,
        offerWeight: mine.weight,
        partnerValue: theirs.value,
        partnerWeight: theirs.weight,
      };
    }),
  );

  constructor(
    private readonly trade: TradeService,
    private readonly sound: SoundService,
  ) {}

  setCategory(category: TradeCategory) {
    this.category$.next(category);
    this.sound.play(Sound.Focus);
  }

  setSearch(value: string) {
    this.search$.next(value);
  }

  /** Clicking an item: a single one goes straight into the offer, a stack asks how many. */
  pick(row: InventoryRow, session: TradeSession) {
    if (session.handingOver) {
      return;
    }
    if (row.item.c === 1) {
      this.trade.setOffer(row.item.i, row.offered ? 0 : 1);
      this.sound.play(Sound.Focus);
      return;
    }
    this.picking = row.item;
    this.pickCount = row.offered || row.item.c;
    this.sound.play(Sound.Focus);
  }

  stepPick(delta: number) {
    if (!this.picking) {
      return;
    }
    this.pickCount = Math.max(
      0,
      Math.min(this.picking.c, this.pickCount + delta),
    );
  }

  setPickCount(value: string | number) {
    if (!this.picking) {
      return;
    }
    const count = Math.floor(Number(value));
    this.pickCount = Number.isFinite(count)
      ? Math.max(0, Math.min(this.picking.c, count))
      : 0;
  }

  confirmPick() {
    if (this.picking) {
      this.trade.setOffer(this.picking.i, this.pickCount);
      this.sound.play(Sound.Ok);
    }
    this.picking = null;
  }

  cancelPick() {
    this.picking = null;
    this.sound.play(Sound.Cancel);
  }

  /** Clicking a row of your offer takes it back out. */
  withdraw(row: OfferRow, session: TradeSession) {
    if (session.handingOver) {
      return;
    }
    this.trade.setOffer(row.item.i, 0);
    this.sound.play(Sound.Cancel);
  }

  toggleReady(session: TradeSession) {
    this.trade.setReady(!session.selfReady);
  }

  cancel() {
    this.trade.cancel();
    this.done.next();
  }

  trackItem(_: number, row: { item: TradeItem }) {
    return row.item.i + ':' + row.item.n;
  }

  @HostListener('window:keydown.escape', ['$event'])
  // @ts-ignore
  private onEscape(event: KeyboardEvent): void {
    event.stopPropagation();
    event.preventDefault();
    if (this.picking) {
      this.cancelPick();
    } else {
      this.cancel();
    }
  }

  @HostListener('window:keydown.enter', ['$event'])
  // @ts-ignore
  private onEnter(event: KeyboardEvent): void {
    if (this.picking) {
      event.stopPropagation();
      event.preventDefault();
      this.confirmPick();
    }
  }
}
