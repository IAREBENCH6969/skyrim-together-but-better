import { Injectable, NgZone, OnDestroy } from '@angular/core';
import { faScaleBalanced } from '@fortawesome/free-solid-svg-icons';
import { BehaviorSubject, Subscription } from 'rxjs';
import { environment } from '../../environments/environment';
import {
  TradeCloseReason,
  TradeItem,
  TradeSession,
} from '../models/trade';
import { View } from '../models/view.enum';
import { UiRepository } from '../store/ui.repository';
import { ClientService } from './client.service';
import { PlayerListService } from './player-list.service';
import { PopupNotificationService } from './popup-notification.service';
import { Sound, SoundService } from './sound.service';

const CLOSE_MESSAGES: Record<TradeCloseReason, string> = {
  [TradeCloseReason.Completed]: 'SERVICE.TRADE.COMPLETED',
  [TradeCloseReason.Cancelled]: 'SERVICE.TRADE.CANCELLED',
  [TradeCloseReason.Declined]: 'SERVICE.TRADE.DECLINED',
  [TradeCloseReason.Busy]: 'SERVICE.TRADE.BUSY',
  [TradeCloseReason.Disconnected]: 'SERVICE.TRADE.DISCONNECTED',
  [TradeCloseReason.Failed]: 'SERVICE.TRADE.FAILED',
  [TradeCloseReason.Expired]: 'SERVICE.TRADE.EXPIRED',
};

const EVENTS = [
  'tradeInvited',
  'tradeOpened',
  'tradePartnerOffer',
  'tradeMyOffer',
  'tradeState',
  'tradeHandingOver',
  'tradeClosed',
] as const;

/** Player-to-player trading: invitations, the open trade, and the trade menu. */
@Injectable({
  providedIn: 'root',
})
export class TradeService implements OnDestroy {
  public readonly session = new BehaviorSubject<TradeSession | null>(null);

  private readonly subscriptions: Subscription[] = [];

  constructor(
    private readonly zone: NgZone,
    private readonly clientService: ClientService,
    private readonly playerListService: PlayerListService,
    private readonly popupNotificationService: PopupNotificationService,
    private readonly uiRepository: UiRepository,
    private readonly sound: SoundService,
  ) {
    skyrimtogether.on('tradeInvited', this.onInvited.bind(this));
    skyrimtogether.on('tradeOpened', this.onOpened.bind(this));
    skyrimtogether.on('tradePartnerOffer', this.onPartnerOffer.bind(this));
    skyrimtogether.on('tradeMyOffer', this.onMyOffer.bind(this));
    skyrimtogether.on('tradeState', this.onState.bind(this));
    skyrimtogether.on('tradeHandingOver', this.onHandingOver.bind(this));
    skyrimtogether.on('tradeClosed', this.onClosed.bind(this));

    // Closing the overlay (F2, Esc) while trading cancels the trade, so nobody is left waiting on a hidden menu.
    this.subscriptions.push(
      this.clientService.activationStateChange.subscribe(active => {
        if (!active && this.session.getValue()) {
          this.cancel();
        }
      }),
    );
  }

  ngOnDestroy(): void {
    for (const event of EVENTS) {
      skyrimtogether.off(event);
    }
    this.subscriptions.forEach(s => s.unsubscribe());
  }

  public invite(playerId: number) {
    skyrimtogether.tradeInvite(playerId);
    this.popupNotificationService.addMessage({
      messageKey: 'SERVICE.TRADE.INVITE_SENT',
      messageParams: { name: this.nameOf(playerId) },
      icon: faScaleBalanced,
      duration: 5000,
    });
  }

  public cancel() {
    skyrimtogether.tradeCancel();
  }

  /** Sets how many of an inventory item (by index) are offered; 0 takes it out of the offer. */
  public setOffer(index: number, count: number) {
    const session = this.session.getValue();
    if (!session || session.handingOver) {
      return;
    }
    const item = session.inventory.find(i => i.i === index);
    if (!item) {
      return;
    }
    const clamped = Math.max(0, Math.min(item.c, Math.floor(count)));
    const offer = { ...session.offer };
    if (clamped > 0) {
      offer[index] = clamped;
    } else {
      delete offer[index];
    }
    // The server resets both "ready" flags when an offer changes; show that straight away.
    this.session.next({
      ...session,
      offer,
      selfReady: false,
      partnerReady: false,
    });
    skyrimtogether.tradeSetOffer(index, clamped);
  }

  public setReady(ready: boolean) {
    const session = this.session.getValue();
    if (!session || session.handingOver) {
      return;
    }
    skyrimtogether.tradeSetReady(ready);
    this.sound.play(ready ? Sound.Check : Sound.Cancel);
  }

  private nameOf(playerId: number): string {
    return this.playerListService.getPlayerById(playerId)?.name ?? '???';
  }

  private log(name: string, args: IArguments) {
    if (environment.game) {
      console.log(
        `%c${name}`,
        'background: #009688; color: #fff; padding: 3px; font-size: 9px;',
        ...Array.from(args).map(v => JSON.stringify(v)),
      );
    }
  }

  private parseItems(json: string): TradeItem[] {
    try {
      const items = JSON.parse(json);
      return Array.isArray(items) ? items : [];
    } catch {
      return [];
    }
  }

  private onInvited(partnerId: number) {
    this.log('onTradeInvited', arguments);
    this.zone.run(() => {
      this.popupNotificationService.addMessage({
        messageKey: 'SERVICE.TRADE.INVITE',
        messageParams: { from: this.nameOf(partnerId) },
        icon: faScaleBalanced,
        duration: 30000,
        actions: [
          {
            nameKey: 'COMPONENT.NOTIFICATIONS.ACCEPT',
            callback: () => skyrimtogether.tradeAccept(partnerId),
          },
          {
            nameKey: 'COMPONENT.NOTIFICATIONS.DECLINE',
            callback: () => skyrimtogether.tradeDecline(partnerId),
          },
        ],
      });
    });
  }

  private onOpened(partnerId: number, inventoryJson: string) {
    this.log('onTradeOpened', arguments);
    this.zone.run(() => {
      this.session.next({
        partnerId,
        partnerName: this.nameOf(partnerId),
        inventory: this.parseItems(inventoryJson),
        offer: {},
        partnerOffer: [],
        selfReady: false,
        partnerReady: false,
        handingOver: false,
      });
      // The invitation has been answered; don't leave its Accept/Decline buttons on screen.
      this.popupNotificationService.clearMessages();
      this.uiRepository.openView(View.TRADE);
      this.sound.play(Sound.Ok);
    });
  }

  private onPartnerOffer(offerJson: string) {
    this.log('onTradePartnerOffer', arguments);
    this.zone.run(() => {
      const session = this.session.getValue();
      if (session) {
        this.session.next({
          ...session,
          partnerOffer: this.parseItems(offerJson),
        });
        this.sound.play(Sound.Focus);
      }
    });
  }

  /** The offer as the game holds it (it may have clamped a count). */
  private onMyOffer(offerJson: string) {
    this.log('onTradeMyOffer', arguments);
    this.zone.run(() => {
      const session = this.session.getValue();
      if (!session) {
        return;
      }
      let offer: Record<number, number> = {};
      try {
        offer = JSON.parse(offerJson) ?? {};
      } catch {
        return;
      }
      this.session.next({ ...session, offer });
    });
  }

  private onState(selfReady: boolean, partnerReady: boolean) {
    this.log('onTradeState', arguments);
    this.zone.run(() => {
      const session = this.session.getValue();
      if (session) {
        if (partnerReady && !session.partnerReady) {
          this.sound.play(Sound.Check);
        }
        this.session.next({ ...session, selfReady, partnerReady });
      }
    });
  }

  private onHandingOver() {
    this.log('onTradeHandingOver', arguments);
    this.zone.run(() => {
      const session = this.session.getValue();
      if (session) {
        this.session.next({ ...session, handingOver: true });
      }
    });
  }

  private onClosed(reason: TradeCloseReason, partnerId: number) {
    this.log('onTradeClosed', arguments);
    this.zone.run(() => {
      const session = this.session.getValue();
      const name = this.nameOf(partnerId || session?.partnerId);
      this.session.next(null);
      if (this.uiRepository.getView() === View.TRADE) {
        this.uiRepository.closeView();
      }
      this.popupNotificationService.addMessage({
        messageKey:
          CLOSE_MESSAGES[reason] ?? CLOSE_MESSAGES[TradeCloseReason.Cancelled],
        messageParams: { name },
        icon: faScaleBalanced,
        duration: 6000,
      });
      this.sound.play(
        reason === TradeCloseReason.Completed ? Sound.Ok : Sound.Cancel,
      );
    });
  }
}
