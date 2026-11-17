import {
  ChangeDetectionStrategy,
  ChangeDetectorRef,
  Component,
  NgZone,
  OnDestroy,
} from '@angular/core';

/** Seconds at which the countdown turns urgent. */
const URGENT_SECONDS = 10;

/**
 * The countdown shown while the local player is down and waiting for a teammate to revive them with a healing
 * potion ("reviveDowned"), and who revived them afterwards ("reviveUp").
 */
@Component({
  selector: 'app-downed',
  templateUrl: './downed.component.html',
  styleUrls: ['./downed.component.scss'],
  changeDetection: ChangeDetectionStrategy.OnPush,
  standalone: false,
})
export class DownedComponent implements OnDestroy {
  /** Whole seconds left, while down. */
  secondsLeft: number | null = null;
  /** 1 when just downed, 0 when the respawn happens. */
  fraction = 1;
  /** Shown for a few seconds after a revive. */
  revivedBy: string | null = null;

  get urgent(): boolean {
    return this.secondsLeft !== null && this.secondsLeft <= URGENT_SECONDS;
  }

  private deadline = 0;
  private total = 1;
  private timer: ReturnType<typeof setInterval> | undefined;
  private revivedTimer: ReturnType<typeof setTimeout> | undefined;

  constructor(
    private readonly zone: NgZone,
    private readonly changeDetector: ChangeDetectorRef,
  ) {
    skyrimtogether.on('reviveDowned', (seconds: number) =>
      this.zone.run(() => this.onDowned(seconds)),
    );
    skyrimtogether.on('reviveUp', (by: string) =>
      this.zone.run(() => this.onUp(by)),
    );
  }

  ngOnDestroy(): void {
    skyrimtogether.off('reviveDowned');
    skyrimtogether.off('reviveUp');
    this.stopTimer();
    clearTimeout(this.revivedTimer);
  }

  private onDowned(seconds: number) {
    this.revivedBy = null;
    this.total = Math.max(1, seconds);
    this.deadline = Date.now() + seconds * 1000;
    this.stopTimer();
    this.tick();
    this.timer = setInterval(() => this.tick(), 100);
  }

  private onUp(by: string) {
    this.stopTimer();
    this.secondsLeft = null;
    if (by) {
      this.revivedBy = by;
      clearTimeout(this.revivedTimer);
      this.revivedTimer = setTimeout(() => {
        this.revivedBy = null;
        this.changeDetector.markForCheck();
      }, 6000);
    }
    this.changeDetector.markForCheck();
  }

  private tick() {
    const left = Math.max(0, (this.deadline - Date.now()) / 1000);
    this.fraction = left / this.total;
    this.secondsLeft = Math.ceil(left);
    if (left <= 0) {
      this.stopTimer();
    }
    this.changeDetector.markForCheck();
  }

  private stopTimer() {
    if (this.timer) {
      clearInterval(this.timer);
      this.timer = undefined;
    }
  }
}
