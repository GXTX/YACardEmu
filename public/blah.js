// Fetch data on first load
let cards = []
let selectedCardName = ''
const carouselInnerDivEl = document.getElementById('inner')
const nextButtonEl = document.getElementById('next')
const cardNameInput = document.getElementById('cardname')

// This is an event listen to all clicks on the carousel arrows.
$('#cardControls').on('slid.bs.carousel', function () {
	const activeCard = carouselInnerDivEl.querySelector('.carousel-item.active')
	selectedCardName = activeCard.dataset.cardName
	updateCardNameInput()
})

function updateCards() {
	// Remove all existing cards.
	while (carouselInnerDivEl.firstChild) {
		carouselInnerDivEl.removeChild(carouselInnerDivEl.firstChild)
	}
	// Insert each card into the carousel.
	cards.forEach(card => {
		const carouselDiv = document.createElement('div')
		// If this is the first card we want to set it as active and set the value of the input to the card name.
		if (card.active == true) {
			carouselDiv.className = "carousel-item active"
			firstCard = false
			selectedCardName = card.name
			updateCardNameInput()
		} else {
			carouselDiv.className = "carousel-item"
		}
		const carouselImage = document.createElement('img')
		carouselImage.src = card.image
		carouselImage.alt = card.name
		carouselDiv.dataset.cardName = card.name
		carouselImage.className = 'd-block w-100 img-responsive center-block'
		carouselDiv.appendChild(carouselImage)
		carouselInnerDivEl.appendChild(carouselDiv);
	})
}

async function fetchCards() {
	await fetch('/api/v1/cards').then(response => {
		if (!response.ok) {
			throw new Error(response.statusText)
		}
		return response.json()
	}).then(value => {
		cards = value
	}).catch(error => {
		console.error(error)
		alert(`Unable to update cards: ${error} : This can happen if you havent created a card.`)
	})
}

function updateCardNameInput() {
	cardNameInput.value = selectedCardName
}

async function loadEverything() {
	// Wait for the cards to be fetched.
	await fetchCards()
	// Update all the cards in the carousel...
	updateCards()
}

loadEverything()
