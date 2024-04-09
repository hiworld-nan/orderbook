In common trading software, real-time display of stock buy and sell order book quotations typically 
involves N levels of the current best pending buy and sell order prices and quantities. We refer to 
this as the N-Order Book.  

Receive the latest quote orders.
Match these orders with the current order book based on order priority.Update the N-Order Book after each match.

Here are the details of the order data fields:
	idx (int): Order number
	time (str): Order timestamp
	price (float): Entrusted price
	volume (int): Entrusted quantity
	quote_type (str): Order type (BID -> Buy order; ASK -> Sell order)
	order_type (str): Order type (LIMIT -> Limit order; MARKET -> Market order)

The rules for order matching are as follows:

Order book priority: [Price priority > Time priority > Order number priority]
Order types:
	Limit order: Specify an acceptable worst execution price 
	  (if it's a buy order, any price <= the specified price can execute; 
	  if it's a sell order, any price >= the specified price can execute).
	Market order: When the order reaches the exchange, it will execute at the best opposing price level 
	  (if it's a buy order, the best opposing price is the lowest price in the current sell order book; 
	  if it's a sell order, the best opposing price is the highest price in the current buy order book).
  
The matching process follows these criteria:
	Price execution condition: If it's a buy order, any opposing order price "less than or equal to" the entrusted price can execute; 
	if it's a sell order, any opposing order price "greater than or equal to" the entrusted price can execute.
	Orders that meet the price execution condition are sorted by order book priority, and their quantities are compared one by one.
	Small orders are considered fully executed and removed from the order book.Large orders are recorded in the order book.
