import MersenneTwister from "mersenne-twister";

export class Random {
    constructor(seed = Date.now()) {
        this.gen = new MersenneTwister(seed);
    }

    int(low, high) {
        if (low > high) {
            [low, high] = [high, low];
        }

        const range = high - low + 1;

        return Math.floor(this.gen.random() * range) + low;
    }

    real(low, high) {
        if (low > high) {
            [low, high] = [high, low];
        }

        return this.gen.random() * (high - low) + low;
    }

    roll(required) {
        return this.int(0, 99) < required;
    }

    rollFloat(required) {
        return this.real(0.0, 1.0) <= required;
    }

    roll0(max) {
        if (max - 1 > 0) {
            return this.int(0, max - 1);
        }
        return 0;
    }

    shuffle(array) {
        for (let i = array.length - 1; i > 0; i--) {
            const j = this.int(0, i);
            [array[i], array[j]] = [array[j], array[i]];
        }
        return array;
    }

    discard(z) {
        if (z === 0) {
            z = 624; // mt19937 state size
        }

        for (let i = 0; i < z; i++) {
            this.gen.random();
        }
    }
}