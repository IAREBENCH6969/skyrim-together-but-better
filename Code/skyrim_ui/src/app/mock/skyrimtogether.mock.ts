import { createStore } from '@ngneat/elf';
import {
  addEntities,
  deleteEntities,
  getAllEntities,
  getEntity,
  selectAllEntities,
  updateEntities,
  withEntities,
} from '@ngneat/elf-entities';
import { EventEmitter } from 'events';
import { fromEvent } from 'rxjs';
import { MessageTypes } from '../services/chat.service';
import { ErrorEvents } from '../services/error.service';
import { MockPlayer } from './mock-player';

let nextPlayerId = 1;

const playerStore = createStore(
  { name: 'players' },
  withEntities<MockPlayer>(),
);

export class SkyrimtogetherMock extends EventEmitter implements SkyrimTogether {
  private connected = false;
  private active = false;
  private version = 'browser';
  private playerName = 'Local Player';
  private showEvents = true;
  private localPlayerId: number;
  public readonly players$ = playerStore.pipe(selectAllEntities());

  connect(host: string, port: number, password: string): void {
    if (!this.connected) {
      let error: ErrorEvents | boolean;
      switch (host) {
        case 't-port':
        case 't-host':
          error = true;
          break;
        case 't-password':
          if (password !== 'test') {
            error = { error: 'wrong_password' };
          }
          break;
        case 't-version':
          error = {
            error: 'wrong_version',
            data: {
              version: '[current-version]',
              expectedVersion: '[expected-version]',
            },
          };
          break;
        case 't-full':
          error = { error: 'server_full' };
          break;
        case 't-client-mods':
          error = { error: 'client_mods_disallowed', data: { mods: ['SKSE'] } };
          break;
        case 't-mods':
          error = {
            error: 'mods_mismatch',
            data: {
              mods: [
                ['missing.esp', '0'],
                ['remove.esp', '12'],
                ['missing_2.esp', '0'],
                ['remove_2.esp', '12'],
              ],
            },
          };
          break;
      }
      setTimeout(() => {
        this.emit(!!error ? 'disconnect' : 'connect');
        this.connected = !error;
        if (error && typeof error !== 'boolean') {
          this.emit('triggerError', JSON.stringify(error));
        } else {
          this.emit('setLocalPlayerId', (this.localPlayerId = nextPlayerId++));
        }
        for (const player of playerStore.query(getAllEntities())) {
          this.emit(
            'playerConnected',
            player.id,
            player.name,
            player.level,
            player.cellName,
          );
          this.emit('setPlayer3dLoaded', player.id, player.health);
        }
      }, 250);
    }
  }

  disconnect(): void {
    if (this.connected) {
      this.emit('disconnect');
      this.connected = false;
    }
  }

  reconnect(): void {
    throw new Error('NOT YET IMPLEMENTED');
  }

  revealPlayers(): void {
    this.sendMessage(MessageTypes.SYSTEM_MESSAGE, "Revealing players...");
  }

  setTime(hours: number, minutes: number): void {
    this.sendMessage(MessageTypes.SYSTEM_MESSAGE, `Setting time to "${hours}:${minutes}"!`);
  }

  sendMessage(type: MessageTypes, message: string): void {
    if (this.connected) {
      this.emit('message', type, message, this.playerName);
    }
  }

  deactivate(): void {
    throw new Error('NOT YET IMPLEMENTED');
  }

  teleportToPlayer(playerId: number): void {
    if (this.connected) {
      const player = playerStore.query(getEntity(playerId));
      if (player) {
        console.log(
          `%cTELEPORT`,
          'background: #F09688; color: #fff; padding: 3px; font-size: 9px;',
          'Teleport to player',
          JSON.stringify(player.name),
          `(${player.id})`,
          'in',
          JSON.stringify(player.cellName),
        );
      }
    }
  }

  launchParty(): void {
    if (this.connected) {
      this.emit('partyCreated');
    }
  }

  createPartyInvite(playerId: number): void {
    playerStore.update(updateEntities(playerId, { invited: true }));
  }

  acceptPartyInvite(inviterId: number): void {
    this.emit('partyCreated');
    this.emit(
      'partyInfo',
      [
        ...playerStore
          .query(getAllEntities())
          .filter(p => p.isInGroup)
          .map(p => p.id),
      ],
      inviterId,
    );
  }

  kickPartyMember(playerId: number): void {
    playerStore.update(updateEntities(playerId, { isInGroup: false }));
    this.emit(
      'partyInfo',
      playerStore
        .query(getAllEntities())
        .filter(p => p.isInGroup)
        .map(p => p.id),
      this.localPlayerId,
    );
  }

  leaveParty(): void {
    if (this.connected) {
      this.emit('partyInfo', [], -1);
      this.emit('partyLeft');
    }
  }

  changePartyLeader(playerId: number): void {
    playerStore.update(updateEntities(playerId, { hasOwnParty: true }));
    this.emit(
      'partyInfo',
      [
        ...playerStore
          .query(getAllEntities())
          .filter(p => p.isInGroup)
          .map(p => p.id),
      ],
      playerId,
    );
  }

  // --- Trading (mock partner: answers after a short delay) ---
  private tradePartner = 0;
  private tradeSelfReady = false;
  private tradeOffer: Record<number, number> = {};
  private readonly tradeInventory = [
    { i: 0, n: 'Gold', c: 1250, v: 1, w: 0, t: 'gold', e: false, p: false },
    { i: 1, n: 'Iron Dagger', c: 2, v: 10, w: 2, t: 'weapons', e: false, p: false },
    { i: 2, n: 'Steel Sword of Burning', c: 1, v: 284, w: 10, t: 'weapons', e: true, p: false },
    { i: 3, n: 'Hide Helmet', c: 1, v: 23, w: 2, t: 'apparel', e: false, p: false },
    { i: 4, n: 'Potion of Fortify Health', c: 3, v: 36, w: 0.5, t: 'potions', e: false, p: false },
    { i: 5, n: 'Poison of Damage Health', c: 2, v: 41, w: 0.5, t: 'potions', e: false, p: false },
    { i: 6, n: 'Blue Mountain Flower', c: 7, v: 2, w: 0.1, t: 'ingredients', e: false, p: false },
    { i: 7, n: 'The Lusty Argonian Maid', c: 1, v: 25, w: 1, t: 'books', e: false, p: false },
    { i: 8, n: 'Petty Soul Gem', c: 4, v: 10, w: 0.1, t: 'misc', e: false, p: false },
    { i: 9, n: 'Iron Ingot', c: 5, v: 7, w: 1, t: 'misc', e: false, p: false },
  ];

  tradeInvite(playerId: number): void {
    setTimeout(() => {
      this.tradePartner = playerId;
      this.tradeOffer = {};
      this.tradeSelfReady = false;
      this.emit('tradeOpened', playerId, JSON.stringify(this.tradeInventory));
      setTimeout(
        () =>
          this.emit(
            'tradePartnerOffer',
            JSON.stringify([
              { i: 0, n: 'Gold', c: 300, v: 1, w: 0, t: 'gold', e: false, p: false },
              { i: 1, n: 'Ebony Dagger', c: 1, v: 290, w: 5, t: 'weapons', e: false, p: true },
            ]),
          ),
        800,
      );
    }, 400);
  }

  tradeAccept(inviterId: number): void {
    this.tradeInvite(inviterId);
  }

  tradeDecline(inviterId: number): void {}

  tradeCancel(): void {
    if (this.tradePartner) {
      const partner = this.tradePartner;
      this.tradePartner = 0;
      this.emit('tradeClosed', 1, partner);
    }
  }

  tradeSetOffer(itemIndex: number, count: number): void {
    this.tradeOffer[itemIndex] = count;
    if (!count) {
      delete this.tradeOffer[itemIndex];
    }
    this.tradeSelfReady = false;
    this.emit('tradeMyOffer', JSON.stringify(this.tradeOffer));
    this.emit('tradeState', false, false);
  }

  tradeSetReady(ready: boolean): void {
    this.tradeSelfReady = ready;
    this.emit('tradeState', ready, false);
    if (ready) {
      setTimeout(() => {
        if (!this.tradeSelfReady || !this.tradePartner) {
          return;
        }
        this.emit('tradeState', true, true);
        this.emit('tradeHandingOver');
        setTimeout(() => {
          const partner = this.tradePartner;
          this.tradePartner = 0;
          this.emit('tradeClosed', 0, partner);
        }, 1200);
      }, 1500);
    }
  }

  mockTradeInvite(playerId: number) {
    this.emit('tradeInvited', playerId);
  }

  initMock() {
    Object.keys((this as any)._events).forEach(e => {
      this.on(e, (...params) => {
        if (this.showEvents) {
          const eventName = e
            .replace(/(\G(?!^)|\b[a-zA-Z][a-z]*)([A-Z][a-z]*|\d+)/gm, `$1_$2`)
            .toUpperCase();
          console.log(
            `%cEVENT`,
            'background: #009688; color: #fff; padding: 3px; font-size: 9px;',
            `[${eventName}]`,
            ...params.map(v => JSON.stringify(v)),
          );
        }
      });
    });
    this.emit('init');
    this.emit('enterGame');
    this.emit('setVersion', this.version);
    this.emit('setName', this.playerName);

    fromEvent(window, 'keydown').subscribe((event: KeyboardEvent) => {
      if (event.ctrlKey && event.location === 2) {
        this.active = !this.active;
        this.emit(this.active ? 'activate' : 'deactivate');
        event.preventDefault();
        return;
      }
      switch (event.key) {
        case 'F2': {
          this.active = !this.active;
          this.emit(this.active ? 'activate' : 'deactivate');
          event.preventDefault();
          break;
        }
      }
    });
  }

  setMockVersion(version: string): void {
    this.version = version;
  }

  setMockPlayerName(name: string): void {
    this.playerName = name;
  }

  setShowEvents(show: boolean): void {
    this.showEvents = show;
  }

  addMockPlayer() {
    const newPlayerId = nextPlayerId++;
    const cities = [
      'Whiterun',
      'Dawnstar',
      'Falkreath',
      'Markarth',
      'Morthal',
      'Riften',
      'Solitude',
      'Windhelm',
    ];
    const newPlayer: MockPlayer = {
      name: 'Player ' + newPlayerId,
      id: newPlayerId,
      level: Math.floor(Math.random() * 100),
      health: Math.floor(Math.random() * 100),
      cellName: cities[Math.floor(Math.random() * cities.length)],
      hasOwnParty: false,
      isInGroup: false,
      invited: false,
      invitedLocalPlayer: false,
    };
    playerStore.update(addEntities(newPlayer));
    if (this.connected) {
      this.emit(
        'playerConnected',
        newPlayer.id,
        newPlayer.name,
        newPlayer.level,
        newPlayer.cellName,
      );
      this.emit('setPlayer3dLoaded', newPlayer.id, newPlayer.health);
    }
    // this.emit('setHealth', newPlayer.id, newPlayer.health);
    return newPlayer;
  }

  disconnectMockPlayer(playerId: number) {
    const mockPlayer = playerStore.query(getEntity(playerId));
    if (mockPlayer) {
      if (this.connected) {
        this.emit('playerDisconnected', mockPlayer.id, mockPlayer.name);
      }
      playerStore.update(deleteEntities(mockPlayer.id));
    }
  }

  accteptMockPlayerInvite(playerId: number) {
    playerStore.update(
      updateEntities(playerId, { invited: false, isInGroup: true }),
    );
    this.emit(
      'partyInfo',
      playerStore
        .query(getAllEntities())
        .filter(p => p.isInGroup)
        .map(p => p.id),
      this.localPlayerId,
    );
  }

  openPlayGuide(): void {
    window.open(
      'https://wiki.tiltedphoques.com/tilted-online/general-information/playguide',
      '_blank',
    );
  }

  mockQuestUpdate() {
    this.emit('questUpdated');
  }

  inviteToPlayerMockParty(playerId: number) {
    playerStore.update(updateEntities(playerId, { invitedLocalPlayer: true }));
    this.emit('partyInviteReceived', playerId);
  }

  startPlayerMockParty(playerId: number) {
    playerStore.update(
      updateEntities(playerId, { hasOwnParty: true, isInGroup: true }),
    );
  }

  mockPlayerLeaveParty(playerId: number) {
    const player = playerStore.query(getEntity(playerId));

    playerStore.update(
      updateEntities(playerId, {
        hasOwnParty: false,
        invitedLocalPlayer: false,
        isInGroup: false,
      }),
    );
    if (this.connected) {
      if (player.isInGroup) {
        this.emit(
          'partyInfo',
          playerStore
            .query(getAllEntities())
            .filter(p => p.isInGroup)
            .map(p => p.id),
          this.localPlayerId,
        );
      } else if (player.hasOwnParty) {
        this.emit('partyInfo', [], -1);
      }
    }
  }

  updateMockDebugData() {
    const debugData = [
      Math.floor(Math.random() * 100),
      Math.floor(Math.random() * 100),
      Math.floor(Math.random() * 100),
      Math.floor((Math.random() / 4) * 100),
      Math.random() * 100,
      Math.random() * 100,
    ];
    this.emit('debugData', ...debugData);
    return debugData;
  }
}

export function mockSkyrimTogether() {
  (globalThis as any).skyrimtogether = new SkyrimtogetherMock();
}
