/**
 * Copyright(C) 2023 Stefano Moioli <smxdev4@gmail.com>
 */

class Dumper {
	#endpoint = "/dumper.php";

	/**
	 * Metro4 progress control
	 */
	#ctrl_progress = null;

	/**
	 * @type {HTMLPreElement}
	 */
	#ctrl_outcome = null;
	/**
	 * @type {HTMLButtonElement}
	 */
	#ctrl_stop = null;
	/**
	 * @type {HTMLInputElement}
	 */
	#ctrl_filename = null;

	/**
	 * Periodic refresh task
	 * @type {number}
	 */
	#interval = null;

	/**
	 * Size of a single sided disk image
	 * @type {number}
	 */
	#FULL_SIZE = 174848;

	constructor(){
		this.#ctrl_progress = Metro.getPlugin(
			document.getElementById('progress'),
			'progress'
		);
		this.#ctrl_outcome = document.getElementById('outcome');
		this.#ctrl_stop = document.getElementById('stop_btn');
		this.#ctrl_filename = document.getElementById('filename');
	}
	
	setOutcome(str){
		this.#ctrl_outcome.textContent = str;
	}

	async start(filename){
		let r = await fetch(this.#endpoint + '?' + new URLSearchParams({
			action: 'start',
			filename: filename
		}));
		return await r.json();
	}

	async stop(){
		if(this.#interval !== null){
			clearInterval(this.#interval);
		}
		await fetch(this.#endpoint + '?' + new URLSearchParams({
			action: 'stop'
		}));
		window.location.reload();
	}

	async poweroff(){
		await fetch(this.#endpoint + '?' + new URLSearchParams({
			action: 'poweroff'
		}));
	}

	async getStatus(){
		let res = await fetch(this.#endpoint + '?' + new URLSearchParams({
			action: 'status'
		}));
		return await res.json();
	}

	async refresh(){
		let s = await this.getStatus();
		if(s.running){
			if(this.#interval === null){
				/**
				 * If there is an active dumper, schedule a periodic refresh
				 * (could use websockets but it would be more complex)
				 */
				this.#interval = setInterval(async () => {
					await this.refresh();
				}, 1000);
			}

			this.#ctrl_stop.style.display = '';
			
			let percent = ((s.size * 100) / this.#FULL_SIZE).toFixed(2);
			this.#ctrl_progress.val(percent);

			/** @type {string} */
			let sizeStr = s.size.toString().padStart(6, ' ');
			this.setOutcome(`${sizeStr} / ${this.#FULL_SIZE} (${percent}%)\nWriting ${s.filename} ...`);
			return s;
		} else {
			if(this.#interval !== null){
				clearInterval(this.#interval);
				this.#interval = null;
			}
			this.setOutcome('Ready');
		}

		let filename = this.#ctrl_filename.value;
		if(filename.length < 1){
			this.#ctrl_outcome.textContent = 'Provide a filename';
			return s;
		}

		return s;
	}

	async main(){
		let filename = this.#ctrl_filename.value;
		let s = await this.start(filename);
		if(!s.running){
			this.setOutcome("Failed to start");
			return;
		}
		await this.refresh();
	}

	static getInstance(){
		if(window.my_dumper) return window.my_dumper;
		window.my_dumper = new Dumper();
		return window.my_dumper;
	}
}

function stop(){
	let d = Dumper.getInstance();
	d.stop();
}

function start(){
	let d = Dumper.getInstance();
	d.main();
}

function refresh(){
	let d = Dumper.getInstance();
	d.refresh();
}

function poweroff(){
	let d = Dumper.getInstance();
	d.poweroff();
}


(function() {
	document.addEventListener("DOMContentLoaded", () => {
		refresh();
	});
})();